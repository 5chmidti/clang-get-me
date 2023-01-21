#include "get_me/path_traversal.hpp"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <list>
#include <memory>
#include <set>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/filter.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/graph.hpp"
#include "support/ranges/functional.hpp"
#include "support/ranges/ranges.hpp"

namespace {
[[nodiscard]] auto createIsValidPathPredicate(const Config &Conf) {
  return [MaxPathLength = Conf.MaxPathLength](const PathType &CurrentPath) {
    return MaxPathLength >= CurrentPath.size();
  };
}

[[nodiscard]] auto createContinuePathSearchPredicate(
    const Config &Conf, const ranges::sized_range auto &CurrentPaths) {
  return [&CurrentPaths, MaxPathCount = Conf.MaxPathCount]() {
    return MaxPathCount > ranges::size(CurrentPaths);
  };
}
} // namespace

class PathFinder {
private:
  [[nodiscard]] auto addToStack() {
    return [this](const EdgeDescriptor Edge) { EdgesStack_.emplace(Edge); };
  };

public:
  PathFinder(const GraphType &Graph, const GraphData &Data, const Config &Conf,
             const VertexDescriptor SourceVertex)
      : Graph_(Graph),
        Data_(Data),
        Conf_(Conf),
        SourceVertex_(SourceVertex),
        State_{SourceVertex},
        Paths_{IsPermutationComparator{Graph_, Data_}} {}

  void operator()() {
    ranges::for_each(toRange(out_edges(SourceVertex_, Graph_)), addToStack());

    const auto IsValidPath = createIsValidPathPredicate(Conf_);
    const auto ContinuePathSearch =
        createContinuePathSearchPredicate(Conf_, Paths_);

    while (!EdgesStack_.empty() && ContinuePathSearch()) {
      const auto Edge = EdgesStack_.top();
      EdgesStack_.pop();

      State_.rollbackPathIfRequired(Source(Edge));
      State_.setCurrentVertex(target(Edge, Graph_));
      State_.addEdge(Edge);

      if (!IsValidPath(State_.getCurrentPath())) {
        continue;
      }

      if (const auto IsFinalVertexInPath = !addOutEdgesOfCurrentVertexToStack();
          IsFinalVertexInPath) {
        Paths_.insert(State_.getCurrentPath());
      }
    }
  }

  [[nodiscard]] std::vector<PathType> commit() {
    return std::move(Paths_) | ranges::to_vector;
  }

private:
  using possible_path_type = std::pair<VertexDescriptor, EdgeDescriptor>;

  class IsPermutationComparator {
  public:
    IsPermutationComparator(const GraphType &Graph, const GraphData &Data)
        : Graph_(Graph),
          Data_(Data) {}

    [[nodiscard]] bool operator()(const PathType &Lhs,
                                  const PathType &Rhs) const {
      if (const auto Comp = Lhs.size() <=> Rhs.size(); std::is_neq(Comp)) {
        return std::is_lt(Comp);
      }
      const auto ToEdgeWeight = [this,
                                 IndexMap = get(boost::edge_index, Graph_)](
                                    const EdgeDescriptor &Edge) {
        return Data_.EdgeWeights[get(IndexMap, Edge)];
      };
      return !ranges::is_permutation(Lhs, Rhs, ranges::equal_to{}, ToEdgeWeight,
                                     ToEdgeWeight);
    }

  private:
    const GraphType &Graph_;
    const GraphData &Data_;
  };

  class StateType {
  public:
    explicit StateType(const VertexDescriptor CurrentVertex)
        : CurrentVertex_(CurrentVertex) {}

    [[nodiscard]] const VertexDescriptor &getCurrentVertex() const {
      return CurrentVertex_;
    }

    [[nodiscard]] const PathType &getCurrentPath() const {
      return CurrentPath_;
    }

    void setCurrentVertex(const VertexDescriptor Vertex) {
      CurrentVertex_ = Vertex;
    }

    void addEdge(const EdgeDescriptor Edge) { CurrentPath_.emplace_back(Edge); }

    void rollbackPathIfRequired(const VertexDescriptor Src) {
      if (requiresRollback(Src)) {
        rollbackTo(Src);
      }
    }

    [[nodiscard]] auto currentPathContainsVertex() const {
      return [this](const VertexDescriptor Vertex) {
        return !ranges::empty(getCurrentPath()) &&
               !(Source(ranges::front(getCurrentPath())) == Vertex) &&
               !ranges::any_of(getCurrentPath(), EqualTo(Vertex), Target);
      };
    }

  private:
    VertexDescriptor CurrentVertex_;
    PathType CurrentPath_{};

    [[nodiscard]] bool requiresRollback(const VertexDescriptor Vertex) const {
      return !CurrentPath_.empty() && CurrentVertex_ != Vertex;
    }

    void rollbackTo(const VertexDescriptor Src) {
      // visiting an edge whose source is not the target of the previous edge.
      // the current path has to be reverted until the new edge can be added
      // to the path remove edges that were added after the path got to src
      CurrentPath_.erase(ranges::find(CurrentPath_, Src, Source),
                         CurrentPath_.end());
    }
  };

  [[nodiscard]] bool addOutEdgesOfCurrentVertexToStack() {
    const auto ToTypeSetSize = [this](const EdgeDescriptor Edge) {
      return Data_.VertexData[Target(Edge)].size();
    };
    const auto VertexToAdd = State_.getCurrentVertex();
    auto FilteredOutEdges =
        toRange(out_edges(VertexToAdd, Graph_)) |
        ranges::views::filter(State_.currentPathContainsVertex(), Target) |
        ranges::to_vector;
    if (ranges::empty(FilteredOutEdges)) {
      return false;
    }
    ranges::for_each(std::move(FilteredOutEdges) |
                         ranges::actions::sort(std::greater{}, ToTypeSetSize),
                     addToStack());
    return true;
  }

  const GraphType &Graph_;
  const GraphData &Data_;
  const Config &Conf_;
  VertexDescriptor SourceVertex_{};

  StateType State_;
  std::stack<EdgeDescriptor> EdgesStack_{};
  std::set<PathType, IsPermutationComparator> Paths_;
};

std::vector<PathType> pathTraversal(const GraphType &Graph,
                                    const GraphData &Data, const Config &Conf,
                                    const VertexDescriptor SourceVertex) {
  auto Finder = PathFinder{Graph, Data, Conf, SourceVertex};
  Finder();
  return Finder.commit();
}

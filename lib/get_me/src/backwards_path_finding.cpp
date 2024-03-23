
#include "get_me/backwards_path_finding.hpp"

#include <cstddef>
#include <functional>
#include <stack>
#include <utility>

#include <range/v3/action/action.hpp>
#include <range/v3/action/reverse.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/iterator/operations.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/filter.hpp>

#include "get_me/graph.hpp"
#include "support/ranges/functional.hpp"

namespace {
template <ranges::range RangeType>
void push(std::stack<TransitionEdgeType> &Stack, RangeType &&Range) {
  ranges::for_each(std::forward<RangeType>(Range),
                   [&Stack](const auto &Value) { Stack.push(Value); });
}

class StateType {
public:
  void addEdge(const TransitionEdgeType &Edge) {
    CurrentPath_.emplace_back(Edge);
  }

  void finishPath() {
    Paths_.emplace(Copy(CurrentPath_) | ranges::actions::reverse);
  }

  [[nodiscard]] bool rollbackPathIfRequired(const TransitionEdgeType &Edge) {
    if (requiresRollback(Edge)) {
      rollbackFor(Edge);
      return true;
    }
    return false;
  }

  [[nodiscard]] bool shouldIgnore(const TransitionEdgeType &Edge) const {
    return ranges::any_of(
        CurrentPath_, [&Edge](const TransitionEdgeType &EdgeInPath) {
          return EdgeInPath.TransitionIndex == Edge.TransitionIndex ||
                 Source(EdgeInPath) == Source(Edge);
        });
  }

  [[nodiscard]] const PathType &getCurrentPath() const { return CurrentPath_; }

  [[nodiscard]] size_t getNumPaths() const { return ranges::size(Paths_); }

  [[nodiscard]] PathContainer takePaths() { return std::move(Paths_); }

private:
  PathContainer Paths_;
  PathType CurrentPath_;

  [[nodiscard]] bool requiresRollback(const TransitionEdgeType &Edge) const {
    return !CurrentPath_.empty() &&
           Source(ranges::back(CurrentPath_)) != Target(Edge);
  }

  void rollbackFor(const TransitionEdgeType &Edge) {
    // visiting an edge whose source is not the target of the previous edge.
    // the current path has to be reverted until the new edge can be added
    // to the path remove edges that were added after the path got to src
    const auto EraseIter = ranges::find(CurrentPath_, Target(Edge), Source);
    const auto End = CurrentPath_.end();
    if (EraseIter != End) {
      CurrentPath_.erase(ranges::next(EraseIter), End);
    } else {
      CurrentPath_.clear();
    }
  }
};

} // namespace

PathContainer runPathFinding(GraphData &Data) {
  const auto &Edges = Data.Edges;

  const auto Roots = getRootVertices(Data);
  const auto Leafs = getLeafVertices(Data);
  const auto TerminatesInLeaf = [&Leafs](const TransitionEdgeType &Step) {
    return ranges::contains(Leafs, Target(Step));
  };
  const auto StartEdges =
      Edges |
      ranges::views::filter(Less(Data.Conf->MaxPathLength),
                            Lookup(Data.VertexDepth, Source)) |
      ranges::views::filter(TerminatesInLeaf) | ranges::to_vector |
      ranges::actions::sort(std::greater{}, Lookup(Data.VertexDepth, Target));

  const auto GetOutEdgesOfVertex =
      [&Edges, &Data](const VertexDescriptor SourceVertex) {
        return Edges | ranges::views::filter(EqualTo(SourceVertex), Target) |
               ranges::to_vector |
               ranges::actions::sort(std::greater{},
                                     Lookup(Data.VertexDepth, Target));
      };

  auto EdgesStack = std::stack<TransitionEdgeType>{};
  push(EdgesStack, StartEdges);

  const auto AddOutEdgesOfVertexToStack =
      [&EdgesStack, GetOutEdgesOfVertex](const VertexDescriptor SourceVertex) {
        push(EdgesStack, GetOutEdgesOfVertex(SourceVertex));
      };

  auto State = StateType{};

  while (!EdgesStack.empty()) {
    const auto Edge = EdgesStack.top();
    EdgesStack.pop();

    if (const auto RolledBack = State.rollbackPathIfRequired(Edge);
        RolledBack) {
      if (Data.Conf->EnableGraphBackwardsEdge &&
          ranges::contains(State.getCurrentPath(), Target(Edge), Target)) {
        continue;
      }
    } else {
      if (Data.Conf->EnableGraphBackwardsEdge &&
          !ranges::empty(State.getCurrentPath()) &&
          Target(ranges::back(State.getCurrentPath())) == Target(Edge)) {
        continue;
      }
    }

    if (State.shouldIgnore(Edge)) {
      continue;
    }

    if (const auto MinPathLengthWhenTakingEdge = SafePlus(
            State.getCurrentPath().size(), Data.VertexDepth[Target(Edge)]);
        MinPathLengthWhenTakingEdge > Data.Conf->MaxPathLength) {
      continue;
    }

    State.addEdge(Edge);

    if (ranges::contains(Roots, Source(Edge))) {
      State.finishPath();
    } else {
      AddOutEdgesOfVertexToStack(Source(Edge));
    }
  }

  return State.takePaths();
}

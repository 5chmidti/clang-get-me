
#include "get_me/backwards_path_finding.hpp"

#include <stack>

#include <boost/container/flat_set.hpp>
#include <range/v3/action/action.hpp>
#include <range/v3/action/reverse.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/iterator/operations.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/filter.hpp>
#include <spdlog/spdlog.h>

#include "get_me/graph.hpp"
#include "support/ranges/functional.hpp"

namespace {
constexpr auto ReversedSource = Target;
constexpr auto ReversedTarget = Source;

template <ranges::range RangeType>
void push(std::stack<TransitionEdgeType> &Stack, RangeType &&Range) {
  ranges::for_each(std::forward<RangeType>(Range),
                   [&Stack](const auto &Value) { Stack.push(Value); });
}

class StateType {
public:
  [[nodiscard]] explicit StateType(GraphData::PathContainer &Paths)
      : Paths_(Paths) {}

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

  [[nodiscard]] const PathType &getCurrentPath() const { return CurrentPath_; }

  [[nodiscard]] size_t getNumPaths() const { return ranges::size(Paths_); }

private:
  GraphData::PathContainer &Paths_;
  PathType CurrentPath_{};

  [[nodiscard]] bool requiresRollback(const TransitionEdgeType &Edge) const {
    return !CurrentPath_.empty() &&
           ReversedTarget(ranges::back(CurrentPath_)) != ReversedSource(Edge);
  }

  void rollbackFor(const TransitionEdgeType &Edge) {
    // visiting an edge whose source is not the target of the previous edge.
    // the current path has to be reverted until the new edge can be added
    // to the path remove edges that were added after the path got to src
    const auto EraseIter =
        ranges::find(CurrentPath_, ReversedSource(Edge), ReversedTarget);
    const auto End = CurrentPath_.end();
    if (EraseIter != End) {
      CurrentPath_.erase(ranges::next(EraseIter), End);
    } else {
      CurrentPath_.clear();
    }
  }
};

} // namespace

void runPathFinding(GraphData &Data) {
  const auto &Edges = Data.Edges;

  const auto TargetVertices = getRootVertices(Data);
  const auto StartVertices = getLeafVertices(Data);
  const auto StartEdges =
      Edges |
      ranges::views::filter(Less(Data.Conf->MaxPathLength),
                            Lookup(Data.VertexDepth, ReversedTarget)) |
      ranges::views::filter([&StartVertices](const TransitionEdgeType &Step) {
        return ranges::contains(StartVertices, ReversedSource(Step));
      }) |
      ranges::to_vector |
      ranges::actions::sort(std::greater{},
                            Lookup(Data.VertexDepth, ReversedSource));

  const auto GetOutEdgesOfVertex =
      [&Edges, &Data](const VertexDescriptor SourceVertex) {
        const auto SourceVertexDepth = Data.VertexDepth[SourceVertex];
        return Edges |
               ranges::views::filter(EqualTo(SourceVertex), ReversedSource) |
               ranges::views::filter(
                   LessEqual(Data.Conf->MaxPathLength),
                   ranges::compose(Plus(SourceVertexDepth),
                                   Lookup(Data.VertexDepth, ReversedTarget))) |
               ranges::to_vector |
               ranges::actions::sort(std::greater{},
                                     Lookup(Data.VertexDepth, ReversedSource));
      };

  auto EdgesStack = std::stack<TransitionEdgeType>{};
  push(EdgesStack, StartEdges);

  const auto AddOutEdgesOfVertexToStack =
      [&EdgesStack, GetOutEdgesOfVertex](const VertexDescriptor SourceVertex) {
        push(EdgesStack, GetOutEdgesOfVertex(SourceVertex));
      };

  auto State = StateType{Data.Paths};

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

    State.addEdge(Edge);

    if (ranges::contains(TargetVertices, ReversedTarget(Edge))) {
      State.finishPath();
    } else {
      AddOutEdgesOfVertexToStack(ReversedTarget(Edge));
    }
  }

  if (Data.VertexData.size() < 10) {
    spdlog::trace("Data: {}", Data);
  }
}

#include "get_me/path_traversal.hpp"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <list>
#include <memory>
#include <set>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>

#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>

#include "get_me/config.hpp"
#include "get_me/graph.hpp"
#include "get_me/utility.hpp"

[[nodiscard]] static auto createIsValidPathPredicate(const Config &Conf) {
  return [MaxPathLength = Conf.MaxPathLength](const PathType &CurrentPath) {
    return MaxPathLength >= CurrentPath.size();
  };
}

[[nodiscard]] static auto createContinuePathSearchPredicate(
    const Config &Conf, const ranges::sized_range auto &CurrentPaths) {
  return [&CurrentPaths, MaxPathCount = Conf.MaxPathCount]() {
    return MaxPathCount > ranges::size(CurrentPaths);
  };
}

// FIXME: there exist paths that contain edges with aliased types and edges with
// their base types, basically creating redundant/non-optimal paths
// FIXME: don't produce paths that end up with the queried type
std::vector<PathType> pathTraversal(const GraphType &Graph,
                                    const GraphData &Data, const Config &Conf,
                                    const VertexDescriptor SourceVertex) {
  using possible_path_type = std::pair<VertexDescriptor, EdgeDescriptor>;

  PathType CurrentPath{};
  const auto IsPermutation = [&Graph, &Data](const PathType &Lhs,
                                             const PathType &Rhs) {
    const auto IndexMap = get(boost::edge_index, Graph);
    const auto ToEdgeWeight = [&IndexMap, &Data](const EdgeDescriptor &Edge) {
      return Data.EdgeWeights[get(IndexMap, Edge)];
    };
    if (const auto Comp = Lhs.size() <=> Rhs.size(); std::is_neq(Comp)) {
      return std::is_lt(Comp);
    }
    return !ranges::is_permutation(Lhs, Rhs, ranges::equal_to{}, ToEdgeWeight,
                                   ToEdgeWeight);
  };
  auto Paths = std::set(std::initializer_list<PathType>{}, IsPermutation);
  std::stack<possible_path_type> EdgesStack{};

  const auto AddToStackFactory = [&EdgesStack](const VertexDescriptor Vertex) {
    return [&EdgesStack, Vertex](const EdgeDescriptor Edge) {
      EdgesStack.emplace(Vertex, Edge);
    };
  };

  ranges::for_each(toRange(out_edges(SourceVertex, Graph)),
                   AddToStackFactory(SourceVertex));

  const auto CurrentPathContainsVertex = [&CurrentPath,
                                          &Graph](const size_t Vertex) {
    const auto HasTargetEdge = [&Graph,
                                Vertex](const EdgeDescriptor &EdgeInPath) {
      return source(EdgeInPath, Graph) == Vertex ||
             target(EdgeInPath, Graph) == Vertex;
    };
    return !ranges::any_of(CurrentPath, HasTargetEdge);
  };
  const auto ToTypeSetSize = [&Data](const EdgeDescriptor Edge) {
    return Data.VertexData[Edge.m_target].size();
  };
  const auto AddOutEdgesOfVertexToStack = [&Graph, &AddToStackFactory,
                                           &CurrentPathContainsVertex,
                                           &ToTypeSetSize](
                                              const VertexDescriptor Vertex) {
    auto FilteredOutEdges = toRange(out_edges(Vertex, Graph)) |
                            ranges::views::filter(CurrentPathContainsVertex,
                                                  &EdgeDescriptor::m_target) |
                            ranges::to_vector;
    if (ranges::empty(FilteredOutEdges)) {
      return false;
    }
    ranges::for_each(std::move(FilteredOutEdges) |
                         ranges::actions::sort(std::greater{}, ToTypeSetSize),
                     AddToStackFactory(Vertex));
    return true;
  };

  const auto IsValidPath = createIsValidPathPredicate(Conf);
  const auto ContinuePathSearch =
      createContinuePathSearchPredicate(Conf, Paths);

  VertexDescriptor CurrentVertex{};
  VertexDescriptor PrevTarget{SourceVertex};
  const auto RequiresRollback = [&CurrentPath,
                                 &PrevTarget](const VertexDescriptor Src) {
    return !CurrentPath.empty() && PrevTarget != Src;
  };
  while (!EdgesStack.empty() && ContinuePathSearch()) {
    const auto [Src, Edge] = EdgesStack.top();
    EdgesStack.pop();

    if (RequiresRollback(Src)) {
      // visiting an edge whose source is not the target of the previous edge.
      // the current path has to be reverted until the new edge can be added to
      // the path
      // remove edges that were added after the path got to src
      CurrentPath.erase(
          ranges::find(CurrentPath, Src, &EdgeDescriptor::m_source),
          CurrentPath.end());
    }
    PrevTarget = target(Edge, Graph);

    CurrentPath.emplace_back(Edge);
    CurrentVertex = target(Edge, Graph);

    if (!IsValidPath(CurrentPath)) {
      continue;
    }

    if (const auto IsFinalVertexInPath =
            !AddOutEdgesOfVertexToStack(CurrentVertex);
        IsFinalVertexInPath) {
      Paths.insert(CurrentPath);
    }
  }

  return ranges::to_vector(Paths);
}

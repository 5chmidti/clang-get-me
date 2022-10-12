#include "get_me/path_traversal.hpp"

#include <stack>

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>

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

  const auto CurrentPathContainsTargetOfEdge =
      [&CurrentPath, &Graph](const EdgeDescriptor Edge) {
        const auto HasTargetEdge = [&Graph, TargetVertex = target(Edge, Graph)](
                                       const EdgeDescriptor &EdgeInPath) {
          return source(EdgeInPath, Graph) == TargetVertex ||
                 target(EdgeInPath, Graph) == TargetVertex;
        };
        return !ranges::any_of(CurrentPath, HasTargetEdge);
      };
  const auto AddOutEdgesOfVertexToStack =
      [&Graph, &AddToStackFactory,
       &CurrentPathContainsTargetOfEdge](const VertexDescriptor Vertex) {
        const auto Vec = ranges::to_vector(toRange(out_edges(Vertex, Graph)));
        auto OutEdgesRange =
            ranges::views::filter(Vec, CurrentPathContainsTargetOfEdge);
        if (OutEdgesRange.empty()) {
          return false;
        }

        ranges::for_each(OutEdgesRange, AddToStackFactory(Vertex));

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
      const auto GetEdgeSource = [&Graph](const EdgeDescriptor &Val) {
        return source(Val, Graph);
      };
      CurrentPath.erase(ranges::find(CurrentPath, Src, GetEdgeSource),
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

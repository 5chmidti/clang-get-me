#include "get_me/graph.hpp"

#include <stack>

#include <fmt/ranges.h>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"

// FIXME: don't produce paths that end up with the queried type
std::vector<PathType> pathTraversal(const GraphType &Graph,
                                    const VertexDescriptor SourceVertex) {
  using ranges::contains;
  using ranges::find;
  using ranges::for_each;
  using possible_path_type = std::pair<
      VertexDescriptor,
      typename boost::graph_traits<GraphType>::out_edge_iterator::value_type>;

  PathType CurrentPath{};
  std::vector<PathType> Paths{};
  std::stack<possible_path_type> EdgesStack{};

  const auto AddOutEdgesOfVertexToStack = [&Graph, &EdgesStack,
                                           &Paths](auto Vertex) {
    auto OutEdgesRange = toRange(out_edges(Vertex, Graph));
    if (OutEdgesRange.empty()) {
      return false;
    }
    spdlog::info(
        "path #{}: adding edges {} to EdgesStack", Paths.size(),
        OutEdgesRange |
            ranges::views::transform(
                [&Graph](const typename boost::graph_traits<
                         GraphType>::out_edge_iterator::value_type &Val) {
                  return std::pair{source(Val, Graph), target(Val, Graph)};
                }));

    const auto AddToStack = [&EdgesStack, Vertex](auto Val) {
      EdgesStack.emplace(Vertex, std::move(Val));
    };
    for_each(std::move(OutEdgesRange), AddToStack);

    return true;
  };

  AddOutEdgesOfVertexToStack(SourceVertex);

  VertexDescriptor CurrentVertex{};
  VertexDescriptor PrevTarget{SourceVertex};
  while (!EdgesStack.empty()) {
    auto PathIndex = Paths.size();
    const auto [Src, Edge] = EdgesStack.top();
    EdgesStack.pop();

    if (contains(CurrentPath, Edge)) {
      spdlog::error("skipping visiting edge already in path");
      continue;
    }

    spdlog::info(
        "path #{}: src: {}, prev target: {}, edge: {}, current path: {}",
        PathIndex, Src, PrevTarget, Edge, CurrentPath);

    if (!CurrentPath.empty() && target(CurrentPath.back(), Graph) != Src) {
      // visiting an edge whose source is not the target of the previous edge.
      // the current path has to be reversed until the new edge can be added to
      // the path
      // remove edges that were added after the path got to src
      const auto Msg =
          fmt::format("path #{}: reverting current path {} back to ", PathIndex,
                      CurrentPath);
      const auto GetEdgeSource = [&Graph](const EdgeDescriptor &Val) {
        return source(Val, Graph);
      };
      CurrentPath.erase(find(CurrentPath, Src, GetEdgeSource),
                        CurrentPath.end());
      spdlog::info("{}{}", Msg, CurrentPath);
      PrevTarget = Src;
    } else {
      PrevTarget = target(Edge, Graph);
    }

    CurrentPath.emplace_back(Edge);
    CurrentVertex = target(Edge, Graph);
    if (const auto IsFinalVertexInPath =
            !AddOutEdgesOfVertexToStack(CurrentVertex);
        IsFinalVertexInPath) {
      Paths.push_back(CurrentPath);
    }

    spdlog::info("path #{}: post algo src: {}, prev target: {}, edge: {}, "
                 "current path: {}",
                 PathIndex, Src, PrevTarget, Edge, CurrentPath);
  }

  return Paths;
}

std::vector<PathType> independentPaths(const std::vector<PathType> &Paths,
                                       const GraphType &Graph) {
  using ranges::any_of;
  using ranges::is_permutation;
  using ranges::views::transform;

  std::vector<PathType> Res{};

  const auto Weightmap = get(boost::edge_weight, Graph);
  const auto ToWeights = [&Weightmap](const EdgeDescriptor &Edge) {
    return get(Weightmap, Edge);
  };

  for (const auto &Path : Paths) {
    if (const auto EquivalentPathContainedInResult =
            any_of(Res,
                   [&Path, &ToWeights](const auto &PathInRes) {
                     return is_permutation(Path | transform(ToWeights),
                                           PathInRes | transform(ToWeights));
                   });
        EquivalentPathContainedInResult) {
      continue;
    }

    Res.push_back(Path);
  }

  return Res;
}

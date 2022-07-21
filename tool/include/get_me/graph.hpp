#ifndef get_me_graph_hpp
#define get_me_graph_hpp

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stack>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <range/v3/algorithm.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/functional/comparisons.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

using namespace std::string_view_literals;

template <typename Graph>
using EdgeDescriptor = typename boost::graph_traits<Graph>::edge_descriptor;

template <typename Graph>
using VertexDescriptor = typename boost::graph_traits<Graph>::vertex_descriptor;

template <typename Graph> using PathType = std::vector<EdgeDescriptor<Graph>>;

template <typename GraphType, typename EdgeWeightType, typename VertexDataType,
          typename EdgeDataType>
struct GraphData {
  using edge_type =
      std::pair<VertexDescriptor<GraphType>, VertexDescriptor<GraphType>>;
  std::vector<edge_type> Edges{};
  std::vector<EdgeWeightType> EdgeWeights{};
  std::map<VertexDescriptor<GraphType>, VertexDataType> VertexData{};
  std::map<EdgeDescriptor<GraphType>, EdgeDataType> EdgeData{};
};

template <typename T>
concept IsGraphData = requires(T Val) {
  Val.VertexData;
  Val.EdgeData;
};

[[nodiscard]] std::ranges::range auto toRange(auto Pair) {
  auto [first, second] = Pair;
  using first_type = decltype(first);
  using second_type = decltype(second);
  struct BeginEnd {
  public:
    BeginEnd(first_type Begin, second_type End)
        : BeginIt(std::move(Begin)), EndIt(std::move(End)) {}
    [[nodiscard]] auto begin() { return BeginIt; }
    [[nodiscard]] auto end() { return EndIt; }
    [[nodiscard]] auto begin() const { return BeginIt; }
    [[nodiscard]] auto end() const { return EndIt; }
    [[nodiscard]] bool empty() const { return begin() == end(); }

  private:
    first_type BeginIt;
    second_type EndIt;
  };
  return BeginEnd{std::move(first), std::move(second)};
}

template <typename GraphType>
[[nodiscard]] std::vector<PathType<GraphType>>
pathTraversal(const GraphType &Graph,
              const VertexDescriptor<GraphType> SourceVertex) {
  using ranges::contains;
  using ranges::find;
  using ranges::for_each;
  using possible_path_type = std::pair<
      VertexDescriptor<GraphType>,
      typename boost::graph_traits<GraphType>::out_edge_iterator::value_type>;

  PathType<GraphType> CurrentPath{};
  std::vector<PathType<GraphType>> Paths{};
  std::stack<possible_path_type> EdgesStack{};

  const auto AddOutEdgesOfVertexToStack = [&Graph, &EdgesStack](auto Vertex) {
    auto OutEdgesRange = to_range(out_edges(Vertex, Graph));
    if (OutEdgesRange.empty()) {
      return false;
    }

    const auto AddToStack = [&EdgesStack, Vertex](auto Val) {
      EdgesStack.emplace(Vertex, std::move(Val));
    };
    for_each(std::move(OutEdgesRange), AddToStack);

    return true;
  };

  AddOutEdgesOfVertexToStack(SourceVertex);

  VertexDescriptor<GraphType> CurrentVertex{};
  VertexDescriptor<GraphType> PrevTarget{SourceVertex};
  while (!EdgesStack.empty()) {
    const auto [src, edge] = EdgesStack.top();
    EdgesStack.pop();

    if (contains(CurrentPath, edge)) {
      spdlog::error(
          "unreachable state detected: CurrentPath contains the current edge "
          "to be visited");
      continue;
    }

    if (auto ContinuesCurrentPath = src == PrevTarget;
        !ContinuesCurrentPath && !CurrentPath.empty()) {
      // visiting an edge whose source is not the target of the previous edge.
      // the current path has to be reversed until the new edge can be added to
      // the path
      // remove edges that were added after the path got to src
      const auto GetEdgeSource =
          [&Graph](const EdgeDescriptor<GraphType> &Val) {
            return source(Val, Graph);
          };
      CurrentPath.erase(find(CurrentPath, source(edge, Graph), GetEdgeSource),
                        CurrentPath.end());

      PrevTarget = src;
    } else {
      PrevTarget = target(edge, Graph);
    }

    CurrentPath.emplace_back(edge);
    CurrentVertex = target(edge, Graph);
    if (const auto IsFinalVertexInPath =
            !AddOutEdgesOfVertexToStack(CurrentVertex);
        IsFinalVertexInPath) {
      Paths.push_back(CurrentPath);
    }
  }

  return Paths;
}

template <typename GraphType>
[[nodiscard]] std::vector<PathType<GraphType>>
independentPaths(const std::vector<PathType<GraphType>> &Paths,
                 const GraphType &Graph) {
  using ranges::any_of;
  using ranges::is_permutation;
  using ranges::views::transform;

  std::vector<PathType<GraphType>> Res{};

  const auto Weightmap = get(boost::edge_weight, Graph);
  const auto ToWeights = [&Weightmap](const EdgeDescriptor<GraphType> &Edge) {
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

#endif

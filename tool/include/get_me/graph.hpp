#ifndef get_me_graph_hpp
#define get_me_graph_hpp

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stack>
#include <string>
#include <string_view>
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

using TypeSetElementType = std::string;
using TypeSet = std::set<TypeSetElementType, std::less<>>;

template <typename Graph>
using EdgeDescriptor = typename boost::graph_traits<Graph>::edge_descriptor;

template <typename Graph>
using VertexDescriptor = typename boost::graph_traits<Graph>::vertex_descriptor;

template <typename Graph> using PathType = std::vector<EdgeDescriptor<Graph>>;

struct LinkType {
  using name_type = std::string;

  name_type Name{};
  name_type TargetName{};
  TypeSet Types{};

  [[nodiscard]] friend bool operator<=>(const LinkType &,
                                        const LinkType &) = default;
};

template <> struct fmt::formatter<LinkType> {
  template <typename FormatContext>
  [[nodiscard]] constexpr auto parse(FormatContext &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const LinkType &Val, FormatContext &Ctx)
      -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "{} {}({})", Val.Name, Val.TargetName,
                          fmt::join(Val.Types, ", "));
  }
};

template <typename GraphType, typename VertexDataType, typename EdgeDataType>
struct GraphData {
  std::map<VertexDescriptor<GraphType>, VertexDataType> VertexData{};
  std::map<EdgeDescriptor<GraphType>, EdgeDataType> EdgeData{};
};

template <typename T>
concept IsGraphData = requires(T Val) {
  Val.VertexData;
  Val.EdgeData;
};

[[nodiscard]] bool isSetIntersectionEmpty(const auto &Iter1, const auto &Iter2,
                                          const IsGraphData auto &Data) {
  using ranges::binary_search;
  using ranges::views::transform;
  if (const auto End = Data.VertexData.end(); Iter1 != End && Iter2 != End) {
    return (Iter1->second | transform([&Iter2](const auto Val) {
              return binary_search(Iter2->second, Val);
            }))
        .empty();
  }
  return true;
};

template <typename GraphType>
[[nodiscard]] bool isEquivalent(const VertexDescriptor<GraphType> &Vertex1,
                                const VertexDescriptor<GraphType> &Vertex2,
                                const IsGraphData auto &Data) {
  const auto DataIter1 = Data.VertexData.find(Vertex1);
  const auto DataIter2 = Data.VertexData.find(Vertex2);
  return ranges::equal(DataIter1->second, DataIter2->second);
};

template <typename GraphType>
[[nodiscard]] bool isOrderDependent(const EdgeDescriptor<GraphType> &Edge1,
                                    const EdgeDescriptor<GraphType> &Edge2,
                                    const GraphType &Graph,
                                    const IsGraphData auto &Data) {
  using boost::source;
  using boost::target;

  return !(is_equivalent(source(Edge1, Graph), target(Edge2, Graph), Data) &&
           is_equivalent(target(Edge1, Graph), source(Edge2, Graph), Data));
}

template <typename GraphType>
[[nodiscard]] bool
isEquivalent(const PathType<GraphType> &Path1, const PathType<GraphType> &Path2,
             const GraphType &Graph, const IsGraphData auto &Data) {
  using ranges::all_of;
  using ranges::views::cartesian_product;
  if (Path1.size() != Path2.size()) {
    return false;
  }

  const auto EdgesAreOrderDependent = [&Data, &Graph](const auto &EdgesPair) {
    return isOrderDependent(std::get<0>(EdgesPair), std::get<1>(EdgesPair),
                            Graph, Data);
  };

  return all_of(cartesian_product(Path1, Path2), EdgesAreOrderDependent);
}

template <typename GraphType>
using edge_ordering =
    std::pair<EdgeDescriptor<GraphType>, EdgeDescriptor<GraphType>>;

template <typename GraphType>
[[nodiscard]] std::optional<edge_ordering<GraphType>>
getOrderDependency(const EdgeDescriptor<GraphType> &Edge1,
                   const EdgeDescriptor<GraphType> &Edge2,
                   const GraphType &Graph, const IsGraphData auto &Data) {
  using boost::source;
  using boost::target;

  if (!is_equivalent(source(Edge1, Graph), target(Edge2, Graph), Data)) {
    return {Edge2, Edge1};
  }
  if (!is_equivalent(target(Edge1, Graph), source(Edge2, Graph), Data)) {
    return {Edge1, Edge2};
  }
  return std::nullopt;
}

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

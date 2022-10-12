#ifndef get_me_include_get_me_graph_hpp
#define get_me_include_get_me_graph_hpp

#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/properties.hpp>
#include <boost/pending/property.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <llvm/ADT/StringRef.h>
#include <range/v3/view/subrange.hpp>

#include "get_me/type_set.hpp"

using namespace std::string_view_literals;

struct DefaultedConstructor {
  const clang::CXXRecordDecl *Record;

  [[nodiscard]] friend auto operator<=>(const DefaultedConstructor &,
                                        const DefaultedConstructor &) = default;
};

using TransitionDataType =
    std::variant<std::monostate, const clang::FunctionDecl *,
                 const clang::FieldDecl *, const clang::VarDecl *,
                 DefaultedConstructor>;

using GraphType =
    boost::adjacency_list<boost::listS, boost::vecS, boost::directedS,
                          boost::no_property,
                          boost::property<boost::edge_index_t, size_t>>;

static_assert(
    std::is_same_v<boost::allow_parallel_edge_tag,
                   boost::graph_traits<GraphType>::edge_parallel_category>,
    "GraphType is required to allow parallel edges");

using EdgeDescriptor = typename boost::graph_traits<GraphType>::edge_descriptor;
using VertexDescriptor =
    typename boost::graph_traits<GraphType>::vertex_descriptor;
using PathType = std::vector<EdgeDescriptor>;

using TransitionType = std::tuple<TypeSet, TransitionDataType, TypeSet>;

using TransitionCollector = std::set<TransitionType>;

struct GraphData {
  using EdgeWeightType = TransitionType;
  using VertexDataType = TypeSet;
  using EdgeType = std::pair<VertexDescriptor, VertexDescriptor>;

  // edges
  std::vector<EdgeType> Edges{};

  // index property of an edge, allows mapping other properties (e.g. weight)
  std::vector<size_t> EdgeIndices{};
  // all possible edge weights
  std::vector<EdgeWeightType> EdgeWeights{};

  // vertices
  std::vector<VertexDataType> VertexData{};
};

[[nodiscard]] std::ranges::viewable_range auto toRange(auto Pair) {
  auto [first, second] = Pair;
  return ranges::subrange{std::move(first), std::move(second)};
}

[[nodiscard]] std::vector<PathType>
pathTraversal(const GraphType &Graph, const GraphData &Data, const Config &Conf,
              VertexDescriptor SourceVertex);

[[nodiscard]] std::pair<GraphType, GraphData>
createGraph(const TransitionCollector &TypeSetTransitionData,
            const std::string &TypeName, const Config &Conf);

[[nodiscard]] TransitionCollector
getTypeSetTransitionData(const TransitionCollector &Collector);

[[nodiscard]] std::optional<VertexDescriptor>
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const std::string &TypeName);

#endif

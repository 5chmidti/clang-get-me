#ifndef get_me_graph_hpp
#define get_me_graph_hpp

#include <map>
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

#include "get_me/type_set.hpp"

using namespace std::string_view_literals;

namespace clang {
struct FunctionDecl;
struct FieldDecl;
struct NamedDecl;
} // namespace clang

// FIXME: make this type something to be usable with more types of custom
// transitions
using CustomTransitionType = std::string;

using TransitionDataType =
    std::variant<std::monostate, const clang::FunctionDecl *,
                 const clang::FieldDecl *, const clang::VarDecl *,
                 CustomTransitionType>;

using GraphType =
    boost::adjacency_list<boost::listS, boost::vecS, boost::directedS,
                          boost::no_property,
                          boost::property<boost::edge_index_t, size_t>>;

using EdgeDescriptor = typename boost::graph_traits<GraphType>::edge_descriptor;
using VertexDescriptor =
    typename boost::graph_traits<GraphType>::vertex_descriptor;
// FIXME: optimize this from pair of edges to list of vertices
using PathType = std::vector<EdgeDescriptor>;

using TypeSetTransitionDataType =
    std::tuple<TypeSet, TransitionDataType, TypeSet>;

using TransitionCollector = std::vector<TypeSetTransitionDataType>;

struct GraphData {
  using EdgeWeightType = TransitionDataType;
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

[[nodiscard]] std::vector<PathType>
pathTraversal(const GraphType &Graph, VertexDescriptor SourceVertex);

[[nodiscard]] std::vector<PathType>
independentPaths(const std::vector<PathType> &Paths, const GraphType &Graph);

[[nodiscard]] std::pair<GraphType, GraphData>
createGraph(const std::vector<TypeSetTransitionDataType> &TypeSetTransitionData,
            const std::string &TypeName);

[[nodiscard]] std::vector<TypeSetTransitionDataType>
getTypeSetTransitionData(const TransitionCollector &Collector);

[[nodiscard]] VertexDescriptor
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const std::string &TypeName);

#endif

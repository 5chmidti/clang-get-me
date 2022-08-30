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

#include "get_me/utility.hpp"

using namespace std::string_view_literals;

namespace clang {
struct FunctionDecl;
struct FieldDecl;
struct NamedDecl;
} // namespace clang

class TransitionCollector;

using TransitionDataType =
    std::variant<std::monostate, const clang::FunctionDecl *,
                 const clang::FieldDecl *>;

using GraphType = boost::adjacency_list<
    boost::listS, boost::vecS, boost::directedS, boost::no_property,
    boost::property<boost::edge_weight_t, TransitionDataType>>;

using EdgeDescriptor = typename boost::graph_traits<GraphType>::edge_descriptor;
using VertexDescriptor =
    typename boost::graph_traits<GraphType>::vertex_descriptor;
using PathType = std::vector<EdgeDescriptor>;

struct TypeValue {
  using meta_type =
      std::variant<std::monostate, clang::QualType, const clang::NamedDecl *>;
  const clang::Type *Value{};
  meta_type MetaValue{};

  [[nodiscard]] friend auto operator==(const TypeValue &Lhs,
                                       const TypeValue &Rhs) {
    return Lhs.Value == Rhs.Value;
  }

  [[nodiscard]] friend auto operator<=>(const TypeValue &Lhs,
                                        const TypeValue &Rhs) {
    return Lhs.Value <=> Rhs.Value;
  }
};

using TypeSetValueType = TypeValue;
using TypeSet = std::set<TypeSetValueType>;

using TypeSetTransitionDataType =
    std::tuple<TypeSet, TransitionDataType, TypeSet>;

struct GraphData {
  using EdgeWeightType = TransitionDataType;
  using VertexDataType = TypeSet;
  using EdgeType = std::pair<VertexDescriptor, VertexDescriptor>;

  // edges
  std::vector<EdgeType> Edges{};

  // all possible edge weights
  std::vector<EdgeWeightType> EdgeWeights{};

  // mapping from edge to weight
  std::map<EdgeType, EdgeWeightType> EdgeWeightMap{};

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

[[nodiscard]] inline auto matchesName(std::string Name) {
  return [Name = std::move(Name)](const TypeSetValueType &Val) {
    const auto NameWithoutNull = std::span{Name.begin(), Name.end() - 1};
    return std::visit(
        Overloaded{[NameWithoutNull](const clang::NamedDecl *NDecl) {
                     const auto NameOfDecl = NDecl->getName();
                     const auto Res = NameOfDecl.contains(llvm::StringRef{
                         NameWithoutNull.data(), NameWithoutNull.size()});
                     return Res;
                   },
                   [&Name](const clang::QualType &QType) {
                     QType->isIntegerType();
                     const auto TypeAsString = QType.getAsString();
                     const auto Res = boost::contains(TypeAsString, Name);
                     return Res;
                   },
                   [](std::monostate) { return false; }},
        Val.MetaValue);
  };
}

[[nodiscard]] GraphData generateVertexAndEdgeWeigths(
    const std::vector<TypeSetTransitionDataType> &TypeSetTransitionData,
    std::string TypeName);

[[nodiscard]] std::vector<TypeSetTransitionDataType>
getTypeSetTransitionData(const TransitionCollector &Collector);

#endif

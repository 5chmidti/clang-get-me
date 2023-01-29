#ifndef get_me_include_get_me_direct_type_dependency_propagation_hpp
#define get_me_include_get_me_direct_type_dependency_propagation_hpp

#include <range/v3/view/concat.hpp>
#include <range/v3/view/for_each.hpp>

#include "get_me/graph.hpp"
#include "get_me/transitions.hpp"

using DTDGraphType =
    boost::adjacency_list<boost::listS, boost::vecS, boost::directedS>;

using EdgeDescriptor =
    typename boost::graph_traits<DTDGraphType>::edge_descriptor;
using VertexDescriptor =
    typename boost::graph_traits<DTDGraphType>::vertex_descriptor;

struct DTDGraphData {
  using EdgeType = std::pair<VertexDescriptor, VertexDescriptor>;

  std::vector<TypeSetValueType> VertexData{};
  std::vector<EdgeType> Edges{};
};

template <typename... Ts> [[nodiscard]] auto propagate(Ts &&...Propagators) {
  return ranges::views::for_each(
      [... Propagators =
           std::forward<Ts>(Propagators)]<typename T>(const T &Value) {
        return ranges::views::concat(Propagators(Value)...);
      });
}

[[nodiscard]] DTDGraphType createGraph(const DTDGraphData &Data);

void propagateTransitionsOfDirectTypeDependencies(
    TransitionCollector &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords,
    const std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls);

#endif

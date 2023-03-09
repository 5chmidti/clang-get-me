#ifndef get_me_lib_get_me_include_get_me_direct_type_dependency_propagation_hpp
#define get_me_lib_get_me_include_get_me_direct_type_dependency_propagation_hpp

#include <utility>
#include <vector>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_selectors.hpp>
#include <boost/graph/graph_traits.hpp>
#include <clang/AST/Decl.h>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/for_each.hpp>

#include "get_me/graph.hpp"
#include "get_me/propagate_type_aliasing.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"

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

void propagateTransitionsOfDirectTypeDependencies(
    TransitionCollector &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords,
    const std::vector<TypeAlias> &TypedefNameDecls, const Config &Conf);

#endif

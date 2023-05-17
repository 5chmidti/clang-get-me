#ifndef get_me_lib_get_me_include_get_me_direct_type_dependency_propagation_hpp
#define get_me_lib_get_me_include_get_me_direct_type_dependency_propagation_hpp

#include <utility>
#include <vector>

#include "get_me/graph.hpp"
#include "get_me/type_set.hpp"

struct DTDGraphData {
  using EdgeType = std::pair<VertexDescriptor, VertexDescriptor>;

  std::vector<TypeSetValueType> VertexData{};
  std::vector<EdgeType> Edges{};
};

#endif

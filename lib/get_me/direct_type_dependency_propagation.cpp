#include "get_me/direct_type_dependency_propagation.hpp"

#include "get_me/propagate_inheritance.hpp"
#include "get_me/propagate_type_aliasing.hpp"

DTDGraphType createGraph(const DTDGraphData &Data) {
  return {Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
          Data.VertexData.size()};
}

void propagateTransitionsOfDirectTypeDependencies(
    TransitionCollector &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords,
    const std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls) {
  propagateInheritance(Transitions, CXXRecords);
  propagateTypeAliasing(Transitions, TypedefNameDecls);
}

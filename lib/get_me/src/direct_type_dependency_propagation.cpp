#include "get_me/direct_type_dependency_propagation.hpp"

#include <vector>

#include <clang/AST/Decl.h>

#include "get_me/config.hpp"
#include "get_me/graph.hpp"
#include "get_me/propagate_inheritance.hpp"
#include "get_me/propagate_type_aliasing.hpp"
#include "get_me/transitions.hpp"

DTDGraphType createGraph(const DTDGraphData &Data) {
  return {Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
          Data.VertexData.size()};
}

void propagateTransitionsOfDirectTypeDependencies(
    TransitionCollector &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords,
    const std::vector<TypeAlias> &TypedefNameDecls, const Config &Conf) {
  if (Conf.EnablePropagateInheritance) {
    propagateInheritance(Transitions, CXXRecords);
  }
  if (Conf.EnablePropagateTypeAlias) {
    propagateTypeAliasing(Transitions, TypedefNameDecls);
  }
}

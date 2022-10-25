#ifndef get_me_include_get_me_direct_type_dependency_propagation_hpp
#define get_me_include_get_me_direct_type_dependency_propagation_hpp

#include "get_me/transitions.hpp"

void propagateTransitionsOfDirectTypeDependencies(
    TransitionCollector &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords,
    const std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls);

#endif

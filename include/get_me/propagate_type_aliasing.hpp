#ifndef get_me_include_get_me_propagate_type_aliasing_hpp
#define get_me_include_get_me_propagate_type_aliasing_hpp

#include "get_me/transitions.hpp"

void propagateTypeAliasing(
    TransitionCollector &Transitions,
    const std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls);

#endif

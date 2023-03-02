#ifndef get_me_lib_get_me_include_get_me_propagate_type_aliasing_hpp
#define get_me_lib_get_me_include_get_me_propagate_type_aliasing_hpp

#include <vector>

#include <clang/AST/Decl.h>

#include "get_me/transitions.hpp"

struct TypeAlias {
  const clang::Type *Base;
  const clang::Type *Alias;
};

void propagateTypeAliasing(TransitionCollector &Transitions,
                           const std::vector<TypeAlias> &TypedefNameDecls);

#endif

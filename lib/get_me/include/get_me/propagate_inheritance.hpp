#ifndef get_me_lib_get_me_include_get_me_propagate_inheritance_hpp
#define get_me_lib_get_me_include_get_me_propagate_inheritance_hpp

#include <vector>

#include <clang/AST/APValue.h>

#include "get_me/transitions.hpp"

void propagateInheritance(
    TransitionData &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords);

#endif

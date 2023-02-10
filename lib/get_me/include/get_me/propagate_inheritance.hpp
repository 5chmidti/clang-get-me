#ifndef get_me_lib_get_me_include_get_me_propagate_inheritance_hpp
#define get_me_lib_get_me_include_get_me_propagate_inheritance_hpp

#include "get_me/transitions.hpp"

void propagateInheritance(
    TransitionCollector &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords);

#endif

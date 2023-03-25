#ifndef get_me_lib_get_me_include_get_me_tooling_hpp
#define get_me_lib_get_me_include_get_me_tooling_hpp

#include <memory>

#include <clang/Frontend/ASTUnit.h>

#include "get_me/config.hpp"
#include "get_me/transitions.hpp"

void collectTransitions(std::shared_ptr<TransitionCollector> Transitions,
                        clang::ASTUnit &AST, const Config &Conf);

#endif

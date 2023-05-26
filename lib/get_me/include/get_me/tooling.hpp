#ifndef get_me_lib_get_me_include_get_me_tooling_hpp
#define get_me_lib_get_me_include_get_me_tooling_hpp

#include <memory>

#include "get_me/config.hpp"
#include "get_me/transitions.hpp"

namespace clang {
class ASTUnit;
}

[[nodiscard]] std::shared_ptr<TransitionData>
collectTransitions(clang::ASTUnit &AST, std::shared_ptr<Config> Conf);

#endif

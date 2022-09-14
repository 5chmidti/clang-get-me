#ifndef get_me_tooling_hpp
#define get_me_tooling_hpp

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <clang/AST/ASTConsumer.h>

#include "get_me/config.hpp"
#include "get_me/graph.hpp"

namespace clang {
struct FunctionDecl;
struct FieldDecl;
} // namespace clang

// FIXME: add support for current context (i.e. current function)
// this would mean only traversing into a function definition if it is the
// current context
class GetMe : public clang::ASTConsumer {
public:
  explicit GetMe(Config Configuration, TransitionCollector &TransitionsRef)
      : Conf{Configuration}, Transitions{TransitionsRef} {}

  void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
  Config Conf;
  TransitionCollector &Transitions;
};

#endif

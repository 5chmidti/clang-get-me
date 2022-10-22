#ifndef get_me_include_get_me_tooling_hpp
#define get_me_include_get_me_tooling_hpp

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <clang/AST/ASTConsumer.h>
#include <clang/Sema/Sema.h>

#include "get_me/config.hpp"
#include "get_me/graph.hpp"

namespace clang {
class FunctionDecl;
class FieldDecl;
class ASTContext;
class Sema;
} // namespace clang

// FIXME: add support for current context (i.e. current function)
// this would mean only traversing into a function definition if it is the
// current context
class GetMe : public clang::ASTConsumer {
public:
  explicit GetMe(const Config &Configuration,
                 TransitionCollector &TransitionsRef, clang::Sema &Sema)
      : Conf_{Configuration}, Transitions_{TransitionsRef}, Sema_{Sema} {}

  void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
  Config Conf_;
  TransitionCollector &Transitions_;
  clang::Sema &Sema_;
};

#endif

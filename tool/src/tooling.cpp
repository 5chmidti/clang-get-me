#include "get_me/tooling.hpp"

#include <memory>

#include <clang/Frontend/FrontendAction.h>
#include <llvm/ADT/StringRef.h>

// FIXME: add support for current context (i.e. current function)
// this would mean only traversing into a function definition if it is the
// current context
class GetMeAction : public clang::ASTFrontendAction {
public:
  explicit GetMeAction(TransitionCollector &Collector)
      : CollectorRef{Collector} {}

  [[nodiscard]] std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance & /*Compiler*/,
                    llvm::StringRef /*InFile*/) override {
    return std::make_unique<GetMe>(CollectorRef);
  }

private:
  TransitionCollector &CollectorRef;
};

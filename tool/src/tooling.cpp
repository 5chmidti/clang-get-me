#include "get_me/tooling.hpp"

void GetMe::HandleTranslationUnit(clang::ASTContext &Context) {
  // Traversing the translation unit decl via a RecursiveASTVisitor
  // will visit all nodes in the AST.
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
}

std::unique_ptr<clang::ASTConsumer>
GetMeAction::CreateASTConsumer(clang::CompilerInstance & /*Compiler*/,
                               llvm::StringRef /*InFile*/) {
  return std::make_unique<GetMe>(CollectorRef);
}

std::unique_ptr<clang::FrontendAction> GetMeActionFactory::create() {
  return std::make_unique<GetMeAction>(CollectorRef);
}

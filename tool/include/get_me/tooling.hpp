#ifndef get_me_tooling_hpp
#define get_me_tooling_hpp

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Tooling.h>

#include "get_me/graph_types.hpp"

struct TransitionCollector {
  std::vector<TransitionDataType> Data{};

  template <typename T>
  void emplace(T &&Value) requires std::convertible_to<T, TransitionDataType> {
    Data.emplace_back(std::forward<T>(Value));
  }
};

class GetMeVisitor : public clang::RecursiveASTVisitor<GetMeVisitor> {
public:
  explicit GetMeVisitor(TransitionCollector &Collector)
      : CollectorRef{Collector} {}

  [[nodiscard]] bool VisitFunctionDecl(clang::FunctionDecl *FDecl) {
    CollectorRef.emplace(FDecl);
    return true;
  }

  [[nodiscard]] bool VisitFieldDecl(clang::FieldDecl *Field) {
    CollectorRef.emplace(Field);
    return true;
  }

private:
  TransitionCollector &CollectorRef;
};

class GetMe : public clang::ASTConsumer {
public:
  explicit GetMe(TransitionCollector &Collector) : Visitor(Collector) {}

  void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
  GetMeVisitor Visitor;
};

class GetMeAction : public clang::ASTFrontendAction {
public:
  explicit GetMeAction(TransitionCollector &Collector)
      : CollectorRef{Collector} {}

  [[nodiscard]] std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance & /*Compiler*/,
                    llvm::StringRef /*InFile*/) override;

private:
  TransitionCollector &CollectorRef;
};

class GetMeActionFactory : public clang::tooling::FrontendActionFactory {
public:
  explicit GetMeActionFactory(TransitionCollector &Collector)
      : CollectorRef(Collector) {}

  [[nodiscard]] std::unique_ptr<clang::FrontendAction> create() override;

private:
  TransitionCollector &CollectorRef;
};

#endif

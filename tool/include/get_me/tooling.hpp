#ifndef get_me_tooling_hpp
#define get_me_tooling_hpp

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "get_me/graph.hpp"

namespace clang {
struct FunctionDecl;
struct FieldDecl;
} // namespace clang

struct TransitionCollector {
  std::vector<TransitionDataType> Data{};

  template <typename T>
  void emplace(T &&Value)
    requires std::convertible_to<T, TransitionDataType>
  {
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
  explicit GetMe(TransitionCollector &Collector) : Visitor{Collector} {}

  void HandleTranslationUnit(clang::ASTContext &Context) override {
    // Traversing the translation unit decl via a RecursiveASTVisitor
    // will visit all nodes in the AST.
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  GetMeVisitor Visitor;
};

#endif

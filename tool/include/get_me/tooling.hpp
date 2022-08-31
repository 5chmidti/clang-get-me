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
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/ranges.h>
#include <llvm/Support/Casting.h>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/mismatch.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/unique.hpp>
#include <range/v3/view/counted.hpp>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"
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

// FIXME: add support for current context (i.e. current function)
// this would mean only traversing into a function definition if it is the
// current context
class GetMeVisitor : public clang::RecursiveASTVisitor<GetMeVisitor> {
public:
  explicit GetMeVisitor(TransitionCollector &Collector)
      : CollectorRef{Collector} {}

  [[nodiscard]] bool VisitFunctionDecl(clang::FunctionDecl *FDecl);

  [[nodiscard]] bool VisitFieldDecl(clang::FieldDecl *Field);

  TransitionCollector &CollectorRef;
};

class GetMe : public clang::ASTConsumer {
public:
  explicit GetMe(TransitionCollector &Collector) : Visitor{Collector} {}

  void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
  GetMeVisitor Visitor;
};

#endif

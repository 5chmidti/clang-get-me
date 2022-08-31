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

class GetMeVisitor : public clang::RecursiveASTVisitor<GetMeVisitor> {
public:
  explicit GetMeVisitor(TransitionCollector &Collector)
      : CollectorRef{Collector} {}

  [[nodiscard]] bool VisitFunctionDecl(clang::FunctionDecl *FDecl) {
    if (FDecl->getDeclName().isIdentifier() &&
        FDecl->getName().startswith("__")) {
      spdlog::info("filtered due to being reserved: {}",
                   FDecl->getNameAsString());
      return true;
    }
    if (!llvm::isa<clang::CXXConstructorDecl>(FDecl) &&
        FDecl->getReturnType()->isVoidType()) {
      spdlog::info("filtered due to returning void: {}",
                   FDecl->getNameAsString());
      return true;
    }
    if (const auto *const Method =
            llvm::dyn_cast<clang::CXXMethodDecl>(FDecl)) {
      // FIXME: support templates
      if (Method->getParent()->isTemplateDecl()) {
        return true;
      }
      if (Method->isDeleted()) {
        return true;
      }
      if (llvm::isa<clang::CXXDestructorDecl>(Method)) {
        return true;
      }
    }
    // FIXME: support templates
    if (FDecl->isTemplateDecl()) {
      return true;
    }

    if (FDecl->getReturnType().getUnqualifiedType().getAsString().starts_with(
            "_")) {
      spdlog::info("filtered due to returning type starting with '_' (): {}",
                   FDecl->getReturnType().getUnqualifiedType().getAsString(),
                   FDecl->getNameAsString());
      return true;
    }

    if (ranges::contains(FDecl->parameters(),
                         FDecl->getReturnType().getUnqualifiedType(),
                         [](const clang::ParmVarDecl *const PVarDecl) {
                           return PVarDecl->getType().getUnqualifiedType();
                         })) {
      spdlog::info("filtered due circular acq/req: {}",
                   FDecl->getNameAsString());
      return true;
    }

    if (ranges::contains(CollectorRef.Data,
                         TransitionDataType{FDecl->getCanonicalDecl()})) {
      return true;
    }

    CollectorRef.emplace(FDecl);
    return true;
  }

  [[nodiscard]] bool VisitFieldDecl(clang::FieldDecl *Field) {
    if (Field->getName().startswith("__")) {
      return true;
    }

    if (Field->getAccess() != clang::AccessSpecifier::AS_public) {
      return true;
    }
    CollectorRef.emplace(Field);
    return true;
  }

  TransitionCollector &CollectorRef;
};

class GetMe : public clang::ASTConsumer {
public:
  explicit GetMe(TransitionCollector &Collector) : Visitor{Collector} {}

  void HandleTranslationUnit(clang::ASTContext &Context) override {
    // Traversing the translation unit decl via a RecursiveASTVisitor
    // will visit all nodes in the AST.
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());

    const auto GetName = [](const TransitionDataType &Val) {
      const auto GetNameOfDeclaratorDecl =
          [](const clang::DeclaratorDecl *const DDecl) -> std::string {
        if (!DDecl->getDeclName().isIdentifier()) {
          if (const auto *const Constructor =
                  llvm::dyn_cast<clang::CXXConstructorDecl>(DDecl)) {
            return fmt::format("constructor of {}",
                               Constructor->getParent()->getNameAsString());
          }
          return "non-identifier";
        }
        return DDecl->getName().str();
      };
      return std::visit(
          Overloaded{GetNameOfDeclaratorDecl,
                     [](std::monostate) -> std::string { return "monostate"; }},
          Val);
    };
    const auto Comparator = [&GetName](const TransitionDataType &Lhs,
                                       const TransitionDataType &Rhs) {
      if (Lhs.index() != Rhs.index()) {
        return Lhs.index() < Rhs.index();
      }
      const auto LhsName = GetName(Lhs);
      const auto RhsName = GetName(Rhs);
      if (LhsName != RhsName) {
        return LhsName < RhsName;
      }
      const auto GetParameters = [](const TransitionDataType &Val) {
        return std::visit(
            Overloaded{
                [](const clang::FunctionDecl *const CurrentDecl)
                    -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
                  return CurrentDecl->parameters();
                },
                [](auto)
                    -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
                  return {};
                }},
            Val);
      };
      const auto LhsParams = GetParameters(Lhs);
      if (!LhsParams) {
        return true;
      }
      const auto RhsParams = GetParameters(Rhs);
      if (!RhsParams) {
        return false;
      }

      const auto MismatchResult =
          ranges::mismatch(LhsParams.value(), RhsParams.value());
      return MismatchResult.in1 == LhsParams.value().end();
    };
    // sort data, this sorts overloads by their number of parameters
    ranges::sort(Visitor.CollectorRef.Data, Comparator);
    const auto UniqueEndIter = ranges::unique(
        Visitor.CollectorRef.Data, [&GetName](const TransitionDataType &Lhs,
                                              const TransitionDataType &Rhs) {
          if (Lhs.index() != Rhs.index()) {
            return false;
          }
          const auto LhsName = GetName(Lhs);
          const auto RhsName = GetName(Rhs);
          const auto Res = LhsName == RhsName;
          spdlog::info("unique name comparison: {} vs {} = {}", LhsName,
                       RhsName, Res);
          return Res;
        });

    auto Res = fmt::format("erasing: [");
    for (auto Iter = UniqueEndIter; Iter != Visitor.CollectorRef.Data.end();
         ++Iter) {
      Res = fmt::format("{}{}, ", Res, *Iter);
    }
    spdlog::info("{}] from {}", Res, Visitor.CollectorRef.Data);
    Visitor.CollectorRef.Data.erase(UniqueEndIter,
                                    Visitor.CollectorRef.Data.end());
  }

private:
  GetMeVisitor Visitor;
};

#endif

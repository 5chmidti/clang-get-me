#include "get_me/transitions.hpp"

#include <clang/AST/DeclCXX.h>
#include <fmt/ranges.h>
#include <range/v3/view/transform.hpp>

#include "support/ranges/ranges.hpp"

namespace {
[[nodiscard]] std::string getTypeAsString(const clang::ValueDecl *const VDecl) {
  return VDecl->getType().getAsString(getNormalizedPrintingPolicy(VDecl));
}

constexpr auto FunctionDeclToStringForAcquired =
    [](const clang::FunctionDecl *const FDecl) {
      if (const auto *const Constructor =
              llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl)) {
        const auto *const Parent = Constructor->getParent();
        return Parent->getNameAsString();
      }
      return FDecl->getReturnType().getAsString(
          getNormalizedPrintingPolicy(FDecl));
    };

constexpr auto FunctionDeclToStringForRequired =
    [](const clang::FunctionDecl *const FDecl) {
      auto Params = fmt::format(
          "{}", fmt::join(FDecl->parameters() |
                              ranges::views::transform(
                                  [](const clang::ParmVarDecl *const PDecl) {
                                    return getTypeAsString(PDecl);
                                  }),
                          ", "));
      if (const auto *const Method =
              llvm::dyn_cast<clang::CXXMethodDecl>(FDecl);
          Method != nullptr && !llvm::isa<clang::CXXConstructorDecl>(Method)) {
        const auto *const RDecl = Method->getParent();
        if (Params.empty()) {
          return RDecl->getNameAsString();
        }
        return fmt::format("{}, {}", RDecl->getNameAsString(), Params);
      }
      return Params;
    };

constexpr auto DeclaratorDeclToString =
    [](const clang::DeclaratorDecl *const DDecl) {
      if (!DDecl->getDeclName().isIdentifier()) {
        if (const auto *const Constructor =
                llvm::dyn_cast<clang::CXXConstructorDecl>(DDecl)) {
          return Constructor->getParent()->getNameAsString();
        }

        auto NonIdentifierString =
            fmt::format("non-identifier {}({})", DDecl->getDeclKindName(),
                        static_cast<const void *>(DDecl));
        return NonIdentifierString;
      }
      return DDecl->getNameAsString();
    };
} // namespace

std::string getTransitionName(const TransitionDataType &Data) {
  return std::visit(DeclaratorDeclToString, Data);
}

std::string getTransitionAcquiredTypeNames(const TransitionDataType &Data) {
  return std::visit(Overloaded{FunctionDeclToStringForAcquired,
                               [](const clang::ValueDecl *const VDecl) {
                                 return getTypeAsString(VDecl);
                               }},
                    Data);
}

std::string getTransitionRequiredTypeNames(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{FunctionDeclToStringForRequired,
                 [](const clang::FieldDecl *const FDecl) {
                   return FDecl->getParent()->getNameAsString();
                 },
                 [](const clang::VarDecl *const /*VDecl*/) -> std::string {
                   return "";
                 }},
      Data);
}

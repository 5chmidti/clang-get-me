#include "get_me/transitions.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <variant>

#include <boost/container/flat_set.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <llvm/Support/Casting.h>
#include <range/v3/algorithm/generate.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>

#include "support/ranges/ranges.hpp" // IWYU pragma: keep
#include "support/variant.hpp"

namespace {
[[nodiscard]] std::string getTypeAsString(const clang::ValueDecl *const VDecl) {
  return VDecl->getType().getAsString();
}

constexpr auto FunctionDeclToStringForAcquired =
    [](const clang::FunctionDecl *const FDecl) {
      if (const auto *const Constructor =
              llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl)) {
        const auto *const Parent = Constructor->getParent();
        return Parent->getNameAsString();
      }
      return FDecl->getReturnType().getAsString();
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

void TransitionData::commit() {
  ranges::generate(Data | ranges::views::transform(ToBundeledTransitionIndex),
                   [Counter = size_t{0U}]() mutable { return Counter++; });
  ranges::generate(Data | ranges::views::values | ranges::views::values |
                       ranges::views::join |
                       ranges::views::transform(ToTransitionIndex),
                   [Counter = size_t{0U}]() mutable { return Counter++; });
  BundeledData =
      Data | ranges::views::transform([](const TransitionType &Transitions) {
        return BundeledTransitionType{
            {ToAcquired(Transitions), ToRequired(Transitions)},
            ToTransitions(Transitions)};
      }) |
      ranges::to<bundeled_container_type>;

  FlatData =
      Data | ranges::views::for_each([](const TransitionType &Transitions) {
        return ToTransitions(Transitions) |
               ranges::views::transform(ToTransition) |
               ranges::views::transform(
                   [Acquired = ToAcquired(Transitions),
                    Required = ToRequired(Transitions)](
                       const TransitionDataType &Transition) {
                     return FlatTransitionType{Acquired, Transition, Required};
                   });
      }) |
      ranges::to<flat_container_type>;
}

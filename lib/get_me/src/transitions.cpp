#include "get_me/transitions.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <boost/container/flat_set.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <llvm/Support/Casting.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/generate.hpp>
#include <range/v3/functional/compose.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>

#include "support/ranges/functional.hpp"
#include "support/ranges/ranges.hpp"
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

std::vector<TransitionType> getSmallestIndependentTransitions(
    const std::vector<TransitionType> &Transitions) {
  auto IndependentTransitions = boost::container::flat_set<TransitionType>{};
  auto Dependencies =
      Transitions |
      ranges::views::transform([&Transitions](const auto &Transition) {
        const auto DependsOn = [](const auto &Dependee) {
          return [&Dependee](const auto &Val) {
            return ToRequired(Val).contains(ToAcquired(Dependee));
          };
        };
        return std::pair{Transition,
                         Transitions |
                             ranges::views::filter(DependsOn(Transition)) |
                             ranges::to<boost::container::flat_set>};
      }) |
      ranges::to_vector |
      ranges::actions::sort(std::less<>{},
                            ranges::compose(ranges::size, Element<1>));

  ranges::for_each(
      Dependencies, [&IndependentTransitions](auto &DependenciesPair) {
        auto &[Transition, DependenciesOfTransition] = DependenciesPair;
        if (ranges::empty(ranges::views::set_intersection(
                IndependentTransitions, DependenciesOfTransition))) {
          IndependentTransitions.emplace(std::move(Transition));
        }
      });

  return IndependentTransitions | ranges::views::move | ranges::to_vector;
}

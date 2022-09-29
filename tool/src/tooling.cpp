#include "get_me/tooling.hpp"

#include <compare>
#include <concepts>
#include <functional>
#include <iterator>
#include <string_view>
#include <type_traits>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include <concepts/concepts.hpp>
#include <fmt/ranges.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/mismatch.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/algorithm/unique.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "get_me/graph.hpp"
#include "get_me/tooling_filters.hpp"
#include "get_me/type_set.hpp"
#include "get_me/utility.hpp"

namespace ranges {
template <>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    enable_borrowed_range<typename clang::CXXRecordDecl::base_class_range> =
        true;
template <>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    enable_borrowed_range<
        typename clang::CXXRecordDecl::base_class_const_range> = true;

template <>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    enable_borrowed_range<typename clang::CXXRecordDecl::method_range> = true;

template <>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    enable_borrowed_range<typename clang::CXXRecordDecl::ctor_range> = true;
} // namespace ranges

static_assert(ranges::viewable_range<
              typename clang::CXXRecordDecl::base_class_const_range>);

template <typename T>
[[nodiscard]] static TransitionType toTransitionType(const T *const Transition,
                                                     const Config &Conf) {

  auto [Acquired, Required] = toTypeSet(Transition, Conf);
  return {std::move(Acquired), TransitionDataType{Transition},
          std::move(Required)};
}

class GetMeVisitor : public clang::RecursiveASTVisitor<GetMeVisitor> {
public:
  GetMeVisitor(const Config &Configuration, TransitionCollector &TransitionsRef,
               std::vector<const clang::CXXRecordDecl *> &CXXRecordsRef,
               std::vector<const clang::TypedefNameDecl *> &TypedefNameDeclsRef,
               clang::Sema &SemaRef)
      : Conf{Configuration}, Transitions{TransitionsRef},
        CxxRecords{CXXRecordsRef},
        TypedefNameDecls{TypedefNameDeclsRef}, Sema{SemaRef} {}

  [[nodiscard]] bool TraverseDecl(clang::Decl *Decl) {
    if (Decl == nullptr) {
      return true;
    }
    if (!Conf.EnableFilterStd || !Decl->isInStdNamespace()) {
      clang::RecursiveASTVisitor<GetMeVisitor>::TraverseDecl(Decl);
    }
    return true;
  }

  [[nodiscard]] bool VisitFunctionDecl(clang::FunctionDecl *FDecl) {
    // handled differently via iterating over a CXXRecord's methods
    if (llvm::isa<clang::CXXMethodDecl>(FDecl)) {
      return true;
    }
    if (filterOut(FDecl, Conf)) {
      return true;
    }

    Transitions.emplace(toTransitionType(FDecl, Conf));
    return true;
  }

  [[nodiscard]] bool VisitFieldDecl(clang::FieldDecl *FDecl) {
    if (hasReservedIdentifierNameOrType(FDecl)) {
      return true;
    }

    // FIXME: filter access spec for members, depends on context of query

    if (FDecl->getType()->isArithmeticType()) {
      return true;
    }
    if (hasTypeNameContainingName(FDecl, "exception")) {
      return true;
    }

    Transitions.emplace(toTransitionType(FDecl, Conf));
    return true;
  }

  [[nodiscard]] bool VisitCXXRecordDecl(clang::CXXRecordDecl *RDecl) {
    if (filterOut(RDecl, Conf)) {
      return true;
    }
    const auto AddToTransitions =
        [this](const ranges::viewable_range auto Range) {
          ranges::transform(
              Range | ranges::views::filter([this](const auto *const Function) {
                return !filterOut(Function, Conf);
              }),
              std::inserter(Transitions, Transitions.end()),
              [this](const auto *const Function) {
                return toTransitionType(Function, Conf);
              });
        };
    AddToTransitions(RDecl->methods());

    // declare implicit default constructor even if it is not used
    if (RDecl->needsImplicitDefaultConstructor()) {
      Sema.DeclareImplicitDefaultConstructor(RDecl);
    }

    AddToTransitions(RDecl->ctors());

    if (RDecl->getNumBases() != 0) {
      CxxRecords.push_back(RDecl);
    }
    return true;
  }

  [[nodiscard]] bool
  VisitTemplateSpecializationType(clang::TemplateSpecializationType *TSType) {
    if (auto *const RDecl = TSType->getAsCXXRecordDecl()) {
      std::ignore = VisitCXXRecordDecl(RDecl);
    }
    return true;
  }

  [[nodiscard]] bool VisitVarDecl(clang::VarDecl *VDecl) {
    if (!VDecl->isStaticDataMember()) {
      return true;
    }
    if (Conf.EnableFilterStd && VDecl->isInStdNamespace()) {
      return true;
    }
    if (VDecl->getType()->isArithmeticType()) {
      return true;
    }
    if (hasReservedIdentifierNameOrType(VDecl)) {
      return true;
    }
    if (hasTypeNameContainingName(VDecl, "exception")) {
      return true;
    }

    Transitions.emplace(std::get<0>(toTypeSet(VDecl, Conf)),
                        TransitionDataType{VDecl}, TypeSet{});
    return true;
  }

  [[nodiscard]] bool VisitTypedefNameDecl(clang::TypedefNameDecl *NDecl) {
    if (NDecl->isInvalidDecl()) {
      return true;
    }
    if (Conf.EnableFilterStd && NDecl->isInStdNamespace()) {
      return true;
    }
    if (NDecl->getUnderlyingType()->isArithmeticType()) {
      return true;
    }
    if (NDecl->isTemplateDecl()) {
      return true;
    }
    if (NDecl->getTypeForDecl() == nullptr ||
        NDecl->getUnderlyingType().getTypePtr() == nullptr) {
      return true;
    }
    if (hasReservedIdentifierNameOrType(NDecl)) {
      return true;
    }
    TypedefNameDecls.push_back(NDecl);
    return true;
  }

private:
  const Config &Conf;
  TransitionCollector &Transitions;
  std::vector<const clang::CXXRecordDecl *> &CxxRecords;
  std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls;
  clang::Sema &Sema;
};

static constexpr auto HasTransitionWithBaseClass = [](const auto &Val) {
  const auto &[Transition, HasBaseInAcquired, HasBaseInRequired, RequiredIter] =
      Val;
  return HasBaseInAcquired || HasBaseInRequired;
};

[[nodiscard]] static auto
toNewTransitionFactory(const clang::Type *const Alias) {
  return [Alias, AliasTS = TypeSet{TypeSetValueType{Alias}}](
             std::tuple<TransitionType, bool, bool, TypeSet::iterator> Val) {
    auto &[Transition, AllowAcquiredConversion, AllowRequiredConversion,
           RequiredIter] = Val;
    auto &[Acquired, _, Required] = Transition;
    if (AllowAcquiredConversion) {
      Acquired = AliasTS;
    }
    if (AllowRequiredConversion) {
      Required.erase(RequiredIter);
      Required.emplace(Alias);
    }
    return Transition;
  };
}

static void filterOverloads(TransitionCollector &Transitions,
                            size_t OverloadFilterParameterCountThreshold = 0) {
  std::vector<TransitionCollector::value_type> Data(
      std::make_move_iterator(Transitions.begin()),
      std::make_move_iterator(Transitions.end()));
  const auto GetParameters = [](const TransitionDataType &Val) {
    return std::visit(
        Overloaded{[](const clang::FunctionDecl *const CurrentDecl)
                       -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
                     return CurrentDecl->parameters();
                   },
                   [](auto &&)
                       -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
                     return {};
                   }},
        Val);
  };
  const auto Comparator =
      [&GetParameters, OverloadFilterParameterCountThreshold](
          const TransitionDataType &Lhs, const TransitionDataType &Rhs) {
        if (const auto IndexComparison = Lhs.index() <=> Rhs.index();
            std::is_neq(IndexComparison)) {
          return std::is_lt(IndexComparison);
        }
        if (const auto NameComparison =
                getTransitionName(Lhs) <=> getTransitionName(Rhs);
            std::is_neq(NameComparison)) {
          return std::is_lt(NameComparison);
        }
        const auto LhsParams = GetParameters(Lhs);
        if (!LhsParams) {
          return true;
        }
        const auto RhsParams = GetParameters(Rhs);
        if (!RhsParams) {
          return false;
        }

        if (LhsParams->empty()) {
          return true;
        }
        if (RhsParams->empty()) {
          return false;
        }

        const auto Projection = [](const clang::ParmVarDecl *const PVarDecl) {
          return PVarDecl->getType();
        };

        if (const auto MismatchResult =
                ranges::mismatch(LhsParams.value(), RhsParams.value(),
                                 std::equal_to{}, Projection, Projection);
            MismatchResult.in1 == LhsParams.value().end()) {
          return static_cast<size_t>(
                     std::distance(LhsParams->begin(), MismatchResult.in1)) <
                 OverloadFilterParameterCountThreshold;
        }
        return false;
      };
  // sort data, this sorts overloads by their number of parameters
  // FIXME: sorting just to make sure the overloads with longer parameter lists
  // are removed, figure out a better way. The algo also depends on this order
  // to determine if it is an overload
  ranges::sort(Data, Comparator,
               [](const TransitionType &Val) { return std::get<1>(Val); });
  const auto IsOverload = [&GetParameters](const TransitionType &LhsTuple,
                                           const TransitionType &RhsTuple) {
    const auto &Lhs = std::get<1>(LhsTuple);
    const auto &Rhs = std::get<1>(RhsTuple);
    if (Lhs.index() != Rhs.index()) {
      return false;
    }
    if (getTransitionName(Lhs) != getTransitionName(Rhs)) {
      return false;
    }
    const auto LhsParams = GetParameters(Lhs);
    if (!LhsParams) {
      return false;
    }
    const auto RhsParams = GetParameters(Rhs);
    if (!RhsParams) {
      return false;
    }

    if (LhsParams->empty()) {
      return false;
    }
    if (RhsParams->empty()) {
      return false;
    }

    const auto Projection = [](const clang::ParmVarDecl *const PVarDecl) {
      return PVarDecl->getType()->getUnqualifiedDesugaredType();
    };
    const auto MismatchResult =
        ranges::mismatch(LhsParams.value(), RhsParams.value(), std::equal_to{},
                         Projection, Projection);
    return MismatchResult.in1 == LhsParams.value().end();
  };
  Data.erase(ranges::unique(Data, IsOverload), Data.end());
  Transitions = TransitionCollector(std::make_move_iterator(Data.begin()),
                                    std::make_move_iterator(Data.end()));
}

template <typename AcquiredClosure, typename RequiredClosure>
  requires std::is_invocable_r_v<bool, AcquiredClosure, TransitionType> &&
           std::is_invocable_r_v<std::pair<TypeSet::iterator, bool>,
                                 RequiredClosure, TypeSet>
[[nodiscard]] auto
toFilterDataFactory(const AcquiredClosure &AllowConversionForAcquired,
                    const RequiredClosure &AllowConversionForRequired) {
  return [&AllowConversionForAcquired,
          &AllowConversionForRequired](TransitionType Val)
             -> std::tuple<TransitionType, bool, bool, TypeSet::iterator> {
    const auto &[Acquired, Transition, Required] = Val;
    const auto [RequiredConversionIter, RequiredConvertionAllowed] =
        AllowConversionForRequired(Required);
    return {std::move(Val), AllowConversionForAcquired(Val),
            RequiredConvertionAllowed, RequiredConversionIter};
  };
}

[[nodiscard]] static TransitionCollector
computePropagatedTransitions(const TransitionCollector &Transitions,
                             const clang::Type *const NewType,
                             const auto &AllowConversionForAcquired,
                             const auto &AllowConversionForRequired) {
  return ranges::to<TransitionCollector>(
      Transitions |
      ranges::views::transform(toFilterDataFactory(
          AllowConversionForAcquired, AllowConversionForRequired)) |
      ranges::views::filter(HasTransitionWithBaseClass) |
      ranges::views::transform(toNewTransitionFactory(NewType)) |
      // FIXME: figure out how to not need this filter
      ranges::views::filter([&Transitions](const TransitionType &Transition) {
        return !ranges::contains(Transitions, Transition);
      }));
}

[[nodiscard]] static const clang::CXXRecordDecl *
getRecordDeclOfConstructorOrNull(const TransitionDataType &Transition) {
  return std::visit(
      Overloaded{
          [](const clang::FunctionDecl *const FDecl)
              -> const clang::CXXRecordDecl * {
            if (const auto *const Constructor =
                    llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl)) {
              return Constructor->getParent();
            }
            return nullptr;
          },
          [](const DefaultedConstructor *DefaultConstructor) {
            return DefaultConstructor->Record;
          },
          [](auto &&) -> const clang::CXXRecordDecl * { return nullptr; }},
      Transition);
}

[[nodiscard]] static auto
propagateInheritanceFactory(TransitionCollector &Transitions) {
  return [&Transitions](const clang::CXXRecordDecl *const RDecl) {
    const auto *const DerivedType = RDecl->getTypeForDecl();
    for (const auto &Base : RDecl->bases()) {
      const auto *const BaseType = launderType(Base.getType().getTypePtr());
      // propagate base -> derived
      auto NewTransitionsForDerived = computePropagatedTransitions(
          Transitions, DerivedType,
          [&BaseType](const TransitionType &Val) {
            if (const clang::CXXRecordDecl *const RecordForConstructor =
                    getRecordDeclOfConstructorOrNull(std::get<1>(Val))) {
              return RecordForConstructor->getTypeForDecl() == BaseType;
            }
            return false;
          },
          [BaseType](const TypeSet &Required) {
            const auto Iter = Required.find(BaseType);
            return std::pair{Iter, Iter != Required.end()};
          });
      Transitions.merge(std::move(NewTransitionsForDerived));

      // propagate derived -> base
      auto NewTransitionsForBase = computePropagatedTransitions(
          Transitions, BaseType,
          [DerivedType](const TransitionType &Val) {
            return std::get<0>(Val) == TypeSet{TypeSetValueType{DerivedType}};
          },
          [](const TypeSet &Required) {
            return std::pair{Required.end(), false};
          });
      Transitions.merge(std::move(NewTransitionsForBase));
    }
    spdlog::trace("propagate inheritance: |Transitions| = {}",
                  Transitions.size());
  };
}

[[nodiscard]] static auto
propagateTypeAliasFactory(TransitionCollector &Transitions,
                          const Config & /*Conf*/) {
  return [&Transitions](const clang::TypedefNameDecl *const NDecl) {
    const auto *const AliasType = launderType(NDecl->getTypeForDecl());
    const auto *const BaseType =
        launderType(NDecl->getUnderlyingType().getTypePtr());
    const auto AddAliasTransitionsForType =
        [&Transitions](const clang::Type *const ExistingType,
                       const clang::Type *const NewType) {
          auto NewTransitions = computePropagatedTransitions(
              Transitions, NewType,
              [ExistingType = TypeSet{TypeSetValueType{ExistingType}}](
                  const TransitionType &Val) {
                return std::get<0>(Val) == ExistingType;
              },
              [ExistingType =
                   TypeSetValueType{ExistingType}](const TypeSet &Required) {
                const auto Iter = Required.find(ExistingType);
                return std::pair{Iter, Iter != Required.end()};
              });
          Transitions.merge(std::move(NewTransitions));
        };
    AddAliasTransitionsForType(BaseType, AliasType);
    AddAliasTransitionsForType(AliasType, BaseType);
  };
}

void GetMe::HandleTranslationUnit(clang::ASTContext &Context) {
  std::vector<const clang::CXXRecordDecl *> CXXRecords{};
  std::vector<const clang::TypedefNameDecl *> TypedefNameDecls{};
  GetMeVisitor Visitor{Conf_, Transitions_, CXXRecords, TypedefNameDecls,
                       Sema_};

  std::ignore = Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  if (Conf_.EnableFilterOverloads) {
    filterOverloads(Transitions_);
  }

  if (Conf_.EnablePropagateInheritance) {
    ranges::for_each(CXXRecords, propagateInheritanceFactory(Transitions_));
  }
  if (Conf_.EnablePropagateTypeAlias) {
    ranges::for_each(TypedefNameDecls,
                     propagateTypeAliasFactory(Transitions_, Conf_));
  }
}

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
               clang::Sema &Sema)
      : Conf_{Configuration}, Transitions_{TransitionsRef},
        CXXRecords_{CXXRecordsRef},
        TypedefNameDecls_{TypedefNameDeclsRef}, Sema_{Sema} {}

  [[nodiscard]] bool TraverseDecl(clang::Decl *Decl) {
    if (!Conf_.EnableFilterStd || !Decl->isInStdNamespace()) {
      clang::RecursiveASTVisitor<GetMeVisitor>::TraverseDecl(Decl);
    }
    return true;
  }

  [[nodiscard]] bool VisitFunctionDecl(clang::FunctionDecl *FDecl) {
    // handled differently via iterating over a CXXRecord's methods
    if (llvm::isa<clang::CXXMethodDecl>(FDecl)) {
      return true;
    }
    if (filterOut(FDecl, Conf_)) {
      return true;
    }

    auto [Acquired, Required] = toTypeSet(FDecl, Conf);
    Transitions.emplace_back(std::move(Acquired), TransitionDataType{FDecl},
                             std::move(Required));
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

    auto [Acquired, Required] = toTypeSet(FDecl, Conf);
    Transitions.emplace_back(std::move(Acquired), TransitionDataType{FDecl},
                             std::move(Required));
    return true;
  }

  [[nodiscard]] bool VisitCXXRecordDecl(clang::CXXRecordDecl *RDecl) {
    // spdlog::info("{}", RDecl->getNameAsString());
    if (filterOut(RDecl, Conf)) {
      return true;
    }
    const auto AddToTransitions =
        [this](const ranges::viewable_range auto Range) {
          ranges::transform(
              Range | ranges::views::filter([this](const auto *const Function) {
                return !filterOut(Function, Conf_);
              }),
              std::inserter(Transitions_, Transitions_.end()),
              [this](const auto *const Function) {
                return toTransitionType(Function, Conf_);
              });
        };
    AddToTransitions(RDecl->methods());

    // declare implicit default constructor even if it is not used
    if (RDecl->needsImplicitDefaultConstructor()) {
      Sema_.DeclareImplicitDefaultConstructor(RDecl);
    }

    AddToTransitions(RDecl->ctors());

    if (RDecl->getNumBases() != 0) {
      CXXRecords_.push_back(RDecl);
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
    if (Conf_.EnableFilterStd && VDecl->isInStdNamespace()) {
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

    if (const auto *const RDecl =
            llvm::dyn_cast<clang::RecordDecl>(VDecl->getDeclContext())) {
      Transitions.emplace_back(std::get<0>(toTypeSet(VDecl, Conf)),
                               TransitionDataType{VDecl}, TypeSet{});
    }
    return true;
  }

  [[nodiscard]] bool VisitTypedefNameDecl(clang::TypedefNameDecl *NDecl) {
    if (NDecl->isInvalidDecl()) {
      return true;
    }
    if (Conf_.EnableFilterStd && NDecl->isInStdNamespace()) {
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
    TypedefNameDecls_.push_back(NDecl);
    return true;
  }

private:
  const Config &Conf_;
  TransitionCollector &Transitions_;
  std::vector<const clang::CXXRecordDecl *> &CXXRecords_;
  std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls_;
  clang::Sema &Sema_;
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
    auto &[Acquired, Function, Required] = Transition;
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
        const auto MismatchResult =
            ranges::mismatch(LhsParams.value(), RhsParams.value(),
                             std::equal_to{}, Projection, Projection);
        const auto Res = MismatchResult.in1 == LhsParams.value().end();
        if (Res) {
          return static_cast<size_t>(
                     std::distance(LhsParams->begin(), MismatchResult.in1)) <
                 OverloadFilterParameterCountThreshold;
        }
        return Res;
      };
  // sort data, this sorts overloads by their number of parameters
  // FIXME: sorting just to make sure the overloads with longer parameter lists
  // are removed, figure out a better way. The algo also depends on this order
  // to determine if it is an overload
  ranges::sort(Data, Comparator,
               [](const auto &Val) { return std::get<1>(Val); });
  const auto IsOverload = [&GetName,
                           &GetParameters](const TransitionType &LhsTuple,
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

[[nodiscard]] static auto
propagateInheritanceFactory(TransitionCollector &Transitions) {
  return [&Transitions](const clang::CXXRecordDecl *const RDecl) {
    const auto *const DerivedType = RDecl->getTypeForDecl();
    const auto BaseTSValue = TypeSetValueType{RDecl->getTypeForDecl()};

    const auto ToFilterData = [&BaseTSValue, RDecl](TransitionType Val)
        -> std::tuple<TransitionType, bool, bool, TypeSet::iterator> {
      const auto &[Acquired, Transition, Required] = Val;
      const auto AllowAcquiredConversion = [&Transition, RDecl]() {
        const auto AllowAcquiredConversionForFunction =
            [RDecl](const clang::FunctionDecl *const FDecl) {
              if (const auto *const Ctor =
                      llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl)) {
                return RDecl->isDerivedFrom(Ctor->getParent());
              }
              if (const auto *const ReturnType =
                      FDecl->getReturnType().getTypePtr();
                  const auto *const ReturnedRecord =
                      ReturnType->getAsCXXRecordDecl()) {
                if (!ReturnedRecord->hasDefinition()) {
                  return false;
                }
                if (ReturnedRecord->getNumBases() == 0) {
                  return false;
                }
                return ReturnedRecord->isDerivedFrom(RDecl);
              }
              return false;
            };
        return std::visit(Overloaded{AllowAcquiredConversionForFunction,
                                     [](auto &&) { return false; }},
                          Transition);
      }();
      const auto AllowRequiredConversion = Required.find(BaseTSValue);
      return {std::move(Val), AllowAcquiredConversion,
              AllowRequiredConversion != Required.end(),
              AllowRequiredConversion};
    };

    auto NewTransitions = ranges::to_vector(
        Transitions | ranges::views::transform(ToFilterData) |
        ranges::views::filter(HasTransitionWithBaseClass) |
        ranges::views::transform(toNewTransitionFactory(DerivedType)) |
        // FIXME: figure out how to not need this filter
        ranges::views::filter([&Transitions](const auto &Transition) {
          return !ranges::contains(Transitions, Transition);
        }));
    Transitions.insert(Transitions.end(),
                       std::make_move_iterator(NewTransitions.begin()),
                       std::make_move_iterator(NewTransitions.end()));
  };
}

[[nodiscard]] static auto
propagateTypeAliasFactory(TransitionCollector &Transitions, Config Conf) {
  return [&Transitions, Conf](const clang::TypedefNameDecl *const NDecl) {
    const auto *const AliasType = launderType(NDecl->getTypeForDecl());
    const auto *const BaseType =
        launderType(NDecl->getUnderlyingType().getTypePtr());

    const auto ToAliasFilterDataFactory = [Conf](
                                              const clang::Type *const Type) {
      return [Type, TypeAsTSValueType = TypeSetValueType{Type},
              Conf](TransitionType Val)
                 -> std::tuple<TransitionType, bool, bool, TypeSet::iterator> {
        const auto &[Acquired, Transition, Required] = Val;
        const auto AllowAcquiredConversion = [Type, &TypeAsTSValueType,
                                              &Transition, Conf]() {
          const auto AllowConversionForFunctionDecl =
              [Type, &TypeAsTSValueType,
               Conf](const clang::FunctionDecl *const FDecl) {
                if (const auto *const Ctor =
                        llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl)) {
                  return Type == Ctor->getParent()->getTypeForDecl();
                }
                if (const auto *const ReturnType =
                        FDecl->getReturnType().getTypePtr()) {
                  return TypeAsTSValueType ==
                         toTypeSetValueType(ReturnType, Conf);
                }
                return false;
              };
          return std::visit(
              Overloaded{AllowConversionForFunctionDecl,
                         [Type](const DefaultedConstructor &Ctor) {
                           return Type == Ctor.Record->getTypeForDecl();
                         },
                         [](auto &&) { return false; }},
              Transition);
        }();
        const auto AllowRequiredConversion = Required.find(TypeAsTSValueType);
        return {std::move(Val), AllowAcquiredConversion,
                AllowRequiredConversion != Required.end(),
                AllowRequiredConversion};
      };
    };
    const auto AddAliasTransitionsForType =
        [&ToAliasFilterDataFactory,
         &Transitions](const clang::Type *const ExistingType,
                       const clang::Type *const NewType) {
          auto NewTransitions = ranges::to_vector(
              Transitions |
              ranges::views::transform(ToAliasFilterDataFactory(ExistingType)) |
              ranges::views::filter(HasTransitionWithBaseClass) |
              ranges::views::transform(toNewTransitionFactory(NewType)) |
              // FIXME: figure out how to not need this filter
              ranges::views::filter([&Transitions](const auto &Transition) {
                return !ranges::contains(Transitions, Transition);
              }));
          Transitions.insert(Transitions.end(),
                             std::make_move_iterator(NewTransitions.begin()),
                             std::make_move_iterator(NewTransitions.end()));
        };
    AddAliasTransitionsForType(BaseType, AliasType);
    AddAliasTransitionsForType(AliasType, BaseType);
  };
}

static void filterArithmeticOverloads(TransitionCollector &Transitions) {
  const auto UniquenessComparator = [](const TransitionType &Lhs,
                                       const TransitionType &Rhs) {
    const auto &[LhsAcquired, LhsFunction, LhsRequired] = Lhs;
    const auto &[RhsAcquired, RhsFunction, RhsRequired] = Rhs;

    if (const auto Comp = LhsAcquired <=> RhsAcquired; std::is_neq(Comp)) {
      return false;
    }
    if (const auto Comp =
            getTransitionName(LhsFunction) <=> getTransitionName(RhsFunction);
        std::is_neq(Comp)) {
      return false;
    }

    const auto IsNotArithmetic = [](const TypeSetValueType &TSet) {
      return TSet.index() != 1;
    };
    return ranges::equal(ranges::views::filter(LhsRequired, IsNotArithmetic),
                         ranges::views::filter(RhsRequired, IsNotArithmetic));
  };
  ranges::sort(Transitions, std::less{});

  Transitions.erase(ranges::unique(Transitions, UniquenessComparator),
                    Transitions.end());
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

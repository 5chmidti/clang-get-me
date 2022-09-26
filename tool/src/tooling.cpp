#include "get_me/tooling.hpp"

#include <compare>
#include <functional>
#include <string_view>
#include <type_traits>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/ranges.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/mismatch.hpp>
#include <range/v3/algorithm/sort.hpp>
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

class GetMeVisitor : public clang::RecursiveASTVisitor<GetMeVisitor> {
public:
  GetMeVisitor(const Config &Configuration, TransitionCollector &TransitionsRef,
               std::vector<const clang::CXXRecordDecl *> &CXXRecordsRef,
               std::vector<const clang::TypedefNameDecl *> &TypedefNameDeclsRef)
      : Conf{Configuration}, Transitions{TransitionsRef},
        CXXRecords{CXXRecordsRef}, TypedefNameDecls{TypedefNameDeclsRef} {}

  [[nodiscard]] bool TraverseDecl(clang::Decl *Decl) {
    if (!Conf.EnableFilterStd || (Decl && !Decl->isInStdNamespace())) {
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
    for (const clang::CXXMethodDecl *Method : RDecl->methods()) {
      // FIXME: allow conversions
      if (llvm::isa<clang::CXXConversionDecl>(Method)) {
        continue;
      }
      if (llvm::isa<clang::CXXDestructorDecl>(Method)) {
        continue;
      }
      if (filterOut(Method, Conf)) {
        continue;
      }

      // FIXME: filter access spec for members, depends on context of query

      auto [Acquired, Required] = toTypeSet(Method, Conf);
      Transitions.emplace_back(std::move(Acquired), TransitionDataType{Method},
                               std::move(Required));
    }
    bool AlreadyAddedDefaultCtor = false;
    for (const clang::CXXConstructorDecl *Constructor : RDecl->ctors()) {
      // FIXME: allow conversions
      if (llvm::isa<clang::CXXConversionDecl>(Constructor)) {
        continue;
      }
      if (llvm::isa<clang::CXXDestructorDecl>(Constructor)) {
        continue;
      }
      if (filterOut(Constructor, Conf)) {
        continue;
      }

      // FIXME: filter access spec for members, depends on context of query

      if (Constructor->isDefaultConstructor()) {
        AlreadyAddedDefaultCtor = true;
      }

      auto [Acquired, Required] = toTypeSet(Constructor, Conf);
      Transitions.emplace_back(std::move(Acquired),
                               TransitionDataType{Constructor},
                               std::move(Required));
    }

    if (!AlreadyAddedDefaultCtor && RDecl->hasDefaultConstructor() &&
        !RDecl->hasUserProvidedDefaultConstructor()) {
      Transitions.emplace_back(
          TypeSet{TypeSetValueType{RDecl->getTypeForDecl()}},
          TransitionDataType{DefaultedConstructor{RDecl}}, TypeSet{});
    }
    CXXRecords.push_back(RDecl);
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
    if (Conf.EnableFilterStd && NDecl->isInStdNamespace()) {
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
  std::vector<const clang::CXXRecordDecl *> &CXXRecords;
  std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls;
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

static void filterOverloads(TransitionCollector &Data,
                            size_t OverloadFilterParameterCountThreshold = 0) {
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
  GetMeVisitor Visitor{Conf, Transitions, CXXRecords, TypedefNameDecls};

  const auto ForceUniquness = [&]() {
    const auto PreUniqueSize = Transitions.size();
    ranges::sort(Transitions);
    Transitions.erase(ranges::unique(Transitions), Transitions.end());
    if (const auto PostUniqueSize = Transitions.size();
        PreUniqueSize > PostUniqueSize) {
      spdlog::warn("PreUniqueSize {} differs from PostUniqueSize {}",
                   PreUniqueSize, PostUniqueSize);
    }
  };

  // FIXME: produces duplicates
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  ForceUniquness();
  if (Conf.EnableFilterOverloads) {
    filterOverloads(Transitions);
    ForceUniquness();
  }
  if (Conf.EnableTruncateArithmetic) {
    filterArithmeticOverloads(Transitions);
    ForceUniquness();
  }

  if (Conf.EnablePropagateInheritance) {
    ranges::for_each(CXXRecords, propagateInheritanceFactory(Transitions));
    ForceUniquness();
  }
  if (Conf.EnablePropagateTypeAlias) {
    // FIXME: produces duplicates
    ranges::for_each(TypedefNameDecls,
                     propagateTypeAliasFactory(Transitions, Conf));
    ForceUniquness();
  }
}

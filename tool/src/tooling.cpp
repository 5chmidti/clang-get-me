#include "get_me/tooling.hpp"

#include <functional>
#include <string_view>
#include <type_traits>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/ranges.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/mismatch.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/unique.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"
#include "get_me/graph.hpp"
#include "get_me/type_set.hpp"
#include "get_me/utility.hpp"

[[nodiscard]] static bool
hasParameterOrReturnTypeWithName(const clang::FunctionDecl *const FDecl,
                                 std::string_view Name) {
  if (FDecl->getReturnType().getUnqualifiedType().getAsString().find(Name) !=
      std::string::npos) {
    return true;
  }
  return ranges::any_of(
      FDecl->parameters(), [Name](const clang::ParmVarDecl *const PVDecl) {
        return PVDecl->getType().getUnqualifiedType().getAsString().find(
                   Name) != std::string::npos;
      });
}

[[nodiscard]] static bool
hasTypeNameContainingName(const clang::ValueDecl *const VDecl,
                          std::string_view Name) {
  return VDecl->getType().getAsString().find(Name) != std::string::npos;
}

[[nodiscard]] static bool hasReservedIdentifier(const clang::QualType &QType) {
  auto QTypeAsString = QType.getAsString();
  return QTypeAsString.starts_with("_") ||
         (QTypeAsString.find("::_") != std::string::npos);
}

[[nodiscard]] static bool
hasReservedIdentifierType(const clang::Type *const Type) {
  return hasReservedIdentifier(clang::QualType(Type, 0));
}

template <typename T>
[[nodiscard]] static bool
hasReservedIdentifierTypeOrReturnType(const T *const Decl) {
  const auto GetReturnTypeOrValueType = [Decl]() -> clang::QualType {
    if constexpr (std::is_same_v<T, clang::FunctionDecl>) {
      return Decl->getReturnType();
    } else if constexpr (std::is_same_v<T, clang::VarDecl> ||
                         std::is_same_v<T, clang::FieldDecl>) {
      return Decl->getType();
    } else if constexpr (std::is_same_v<T, clang::TypedefNameDecl>) {
      return clang::QualType(Decl->getTypeForDecl(), 0);
    } else {
      static_assert(std::is_same_v<T, void>,
                    "hasReservedIdentifierType called with unsupported type");
    }
  };
  return hasReservedIdentifier(GetReturnTypeOrValueType());
}

template <typename T>
[[nodiscard]] static bool hasReservedIdentifierNameOrType(const T *const Decl) {
  if (Decl->getDeclName().isIdentifier() && Decl->getName().startswith("_")) {
    return true;
  }
  if constexpr (std::is_same_v<T, clang::FunctionDecl> ||
                std::is_same_v<T, clang::VarDecl> ||
                std::is_same_v<T, clang::FieldDecl>) {
    return hasReservedIdentifierTypeOrReturnType(Decl);
  }
  if constexpr (std::is_same_v<T, clang::TypedefNameDecl>) {
    return hasReservedIdentifierTypeOrReturnType(Decl) ||
           hasReservedIdentifierType(Decl->getUnderlyingType().getTypePtr());
  }
  return false;
}

[[nodiscard]] static bool
returnTypeIsInParameterList(const clang::FunctionDecl *const FDecl) {
  return ranges::contains(FDecl->parameters(),
                          FDecl->getReturnType().getUnqualifiedType(),
                          [](const clang::ParmVarDecl *const PVarDecl) {
                            return PVarDecl->getType().getUnqualifiedType();
                          });
}

[[nodiscard]] static bool
filterFunction(const clang::FunctionDecl *const FDecl) {
  if (hasReservedIdentifierNameOrType(FDecl)) {
    return true;
  }
  if (hasParameterOrReturnTypeWithName(FDecl, "FILE")) {
    return true;
  }
  if (hasParameterOrReturnTypeWithName(FDecl, "exception")) {
    return true;
  }
  if (hasParameterOrReturnTypeWithName(FDecl, "bad_array_new_length")) {
    return true;
  }
  if (hasParameterOrReturnTypeWithName(FDecl, "bad_alloc")) {
    return true;
  }
  if (hasParameterOrReturnTypeWithName(FDecl, "traits")) {
    return true;
  }
  // FIXME: maybe need heuristic to reduce unwanted edges
  if (FDecl->getReturnType()->isArithmeticType()) {
    spdlog::trace("filtered due to returning arithmetic type: {}",
                  FDecl->getNameAsString());
    return true;
  }
  if (returnTypeIsInParameterList(FDecl)) {
    spdlog::trace("filtered due to require-acquire cycle: {}",
                  FDecl->getNameAsString());
    return true;
  }
  // FIXME: support templates
  if (FDecl->isTemplateDecl()) {
    return true;
  }

  return false;
}

[[nodiscard]] static bool hasAnyName(const clang::NamedDecl *const NDecl,
                                     ranges::range auto RangeOfNames) {
  return ranges::any_of(
      RangeOfNames, [NameOfDecl = NDecl->getNameAsString()](const auto &Name) {
        return NameOfDecl.find(Name) != std::string::npos;
      });
}

[[nodiscard]] static bool
filterCXXRecord(const clang::CXXRecordDecl *const RDecl) {
  if (RDecl != RDecl->getDefinition()) {
    return true;
  }
  if (RDecl->isDependentType()) {
    spdlog::trace("filtered due to being dependent type: {}",
                  RDecl->getNameAsString());
    return true;
  }
  if (RDecl->isTemplateDecl()) {
    spdlog::trace("filtered due to being template decl: {}",
                  RDecl->getNameAsString());
    return true;
  }
  if (hasReservedIdentifierNameOrType(RDecl)) {
    return true;
  }
  if (RDecl->getNameAsString().empty()) {
    spdlog::trace("filtered due to having empty name");
    return true;
  }
  if (hasAnyName(RDecl,
                 std::array{"FILE"sv, "exception"sv, "bad_array_new_length"sv,
                            "bad_alloc"sv, "traits"sv})) {
    return true;
  }

  return false;
}

// FIXME: skip TransitionCollector and generate
// std::vector<TypeSetTransitionDataType -> GraphData in the visitor directly
class GetMeVisitor : public clang::RecursiveASTVisitor<GetMeVisitor> {
public:
  GetMeVisitor(Config Configuration, TransitionCollector &TransitionsRef,
               std::vector<const clang::CXXRecordDecl *> &CXXRecordsRef,
               std::vector<const clang::TypedefNameDecl *> &TypedefNameDeclsRef)
      : Conf{Configuration}, Transitions{TransitionsRef},
        CXXRecords{CXXRecordsRef}, TypedefNameDecls{TypedefNameDeclsRef} {}

  [[nodiscard]] bool VisitFunctionDecl(clang::FunctionDecl *FDecl) {
    if (llvm::isa<clang::CXXMethodDecl>(FDecl)) {
      return true;
    }
    if (filterFunction(FDecl)) {
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

    if (FDecl->getAccess() != clang::AccessSpecifier::AS_public) {
      return true;
    }

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
    if (filterCXXRecord(RDecl)) {
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
      if (Method->isDeleted()) {
        continue;
      }
      if (filterFunction(Method)) {
        continue;
      }

      auto [Acquired, Required] = toTypeSet(Method, Conf);
      Transitions.emplace_back(std::move(Acquired), TransitionDataType{Method},
                               std::move(Required));
    }

    // add non user provided default constructor
    if (RDecl->hasDefaultConstructor() &&
        !RDecl->hasUserProvidedDefaultConstructor()) {
      Transitions.emplace_back(
          TypeSet{TypeSetValueType{RDecl->getTypeForDecl()}},
          TransitionDataType{DefaultedConstructor{RDecl}}, TypeSet{});
    }
    CXXRecords.push_back(RDecl);
    return true;
  }

  [[nodiscard]] bool VisitVarDecl(clang::VarDecl *VDecl) {
    if (!VDecl->isStaticDataMember()) {
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

  Config Conf;
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

[[nodiscard]] static auto
propagateInheritanceFactory(TransitionCollector &Transitions) {
  return [&Transitions](const clang::CXXRecordDecl *const RDecl) {
    if (filterCXXRecord(RDecl)) {
      return;
    }
    const auto *const DerivedType = RDecl->getTypeForDecl();
    const auto DerivedTSValue = TypeSetValueType{DerivedType};
    const auto DerivedTS = TypeSet{DerivedTSValue};
    const auto BaseTSValue = TypeSetValueType{RDecl->getTypeForDecl()};

    const auto ToFilterData = [&BaseTSValue, RDecl](TransitionType Val)
        -> std::tuple<TransitionType, bool, bool, TypeSet::iterator> {
      const auto &[Acquired, Transition, Required] = Val;
      const auto AllowAcquiredConversion = [&Transition, RDecl]() {
        return std::visit(
            Overloaded{[RDecl](const clang::FunctionDecl *const FDecl) {
                         if (const auto *const Ctor =
                                 llvm::dyn_cast<clang::CXXConstructorDecl>(
                                     FDecl)) {
                           return RDecl->isDerivedFrom(Ctor->getParent());
                         }
                         if (const auto *const ReturnType =
                                 FDecl->getReturnType().getTypePtr();
                             const auto *const ReturnedRecord =
                                 ReturnType->getAsCXXRecordDecl()) {
                           return ReturnedRecord->isDerivedFrom(RDecl);
                         }
                         return false;
                       },
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
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  if (Conf.EnableArithmeticTruncation) {
    filterArithmeticOverloads(Transitions);
  }

  ranges::for_each(CXXRecords, propagateInheritanceFactory(Transitions));
  ranges::for_each(TypedefNameDecls,
                   propagateTypeAliasFactory(Transitions, Conf));
}

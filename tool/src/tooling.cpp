#include "get_me/tooling.hpp"

#include <functional>
#include <string_view>
#include <type_traits>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/ranges.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
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

template <typename T>
[[nodiscard]] static bool
hasReservedIdentifierTypeOrReturnType(const T *const Decl) {
  const auto GetReturnTypeOrValueType = [Decl]() {
    if constexpr (std::is_same_v<T, clang::FunctionDecl>) {
      return Decl->getReturnType();
    } else if constexpr (std::is_same_v<T, clang::VarDecl> ||
                         std::is_same_v<T, clang::FieldDecl>) {
      return Decl->getType();
    } else {
      static_assert(std::is_same_v<T, void>,
                    "hasReservedIdentifierType called with unsupported type");
    }
  };
  return GetReturnTypeOrValueType().getAsString().starts_with("_");
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

  return false;
}

// FIXME: skip TransitionCollector and generate
// std::vector<TypeSetTransitionDataType -> GraphData in the visitor directly
class GetMeVisitor : public clang::RecursiveASTVisitor<GetMeVisitor> {
public:
  explicit GetMeVisitor(TransitionCollector &Collector)
      : Collector{Collector} {}

  [[nodiscard]] bool VisitFunctionDecl(clang::FunctionDecl *FDecl) {
    if (llvm::isa<clang::CXXMethodDecl>(FDecl)) {
      return true;
    }
    if (filterFunction(FDecl)) {
      return true;
    }

    auto [Acquired, Required] = toTypeSet(FDecl);
    Collector.emplace_back(std::move(Acquired), TransitionDataType{FDecl},
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

    auto [Acquired, Required] = toTypeSet(FDecl);
    Collector.emplace_back(std::move(Acquired), TransitionDataType{FDecl},
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

      auto [Acquired, Required] = toTypeSet(Method);
      Collector.emplace_back(std::move(Acquired), TransitionDataType{Method},
                             std::move(Required));
    }

    // add non user provided default constructor
    if (RDecl->hasDefaultConstructor() &&
        !RDecl->hasUserProvidedDefaultConstructor()) {
      Collector.emplace_back(TypeSet{TypeSetValueType{RDecl->getTypeForDecl()}},
                             TransitionDataType{RDecl->getNameAsString()},
                             TypeSet{});
    }
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
      Collector.emplace_back(
          TypeSet{TypeSetValueType{
              VDecl->getType().getCanonicalType().getTypePtr()}},
          TransitionDataType{VDecl}, TypeSet{});
    }
    return true;
  }

  TransitionCollector &Collector;
};

class GetMePostVisitor : public clang::RecursiveASTVisitor<GetMePostVisitor> {
public:
  explicit GetMePostVisitor(TransitionCollector &Collector)
      : Collector{Collector} {}

  [[nodiscard]] bool VisitCXXRecordDecl(clang::CXXRecordDecl *RDecl) {
    if (filterCXXRecord(RDecl)) {
      return true;
    }
    const auto DerivedTSValue = TypeSetValueType{RDecl->getTypeForDecl()};
    const auto DerivedTS = TypeSet{DerivedTSValue};
    const auto BaseTSValue = TypeSetValueType{RDecl->getTypeForDecl()};
    const auto HasTransitionWithBaseClass = [](const auto &Val) {
      const auto &[Transition, HasBaseInAcquired, HasBaseInRequired,
                   RequiredIter] = Val;
      return HasBaseInAcquired || HasBaseInRequired;
    };

    const auto TransformToFilterData = [&BaseTSValue,
                                        RDecl](TypeSetTransitionDataType Val)
        -> std::tuple<TypeSetTransitionDataType, bool, bool,
                      TypeSet::iterator> {
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

    const auto FilterDataToNewTransition =
        [&DerivedTS, &DerivedTSValue](
            std::tuple<TypeSetTransitionDataType, bool, bool, TypeSet::iterator>
                Val) {
          auto &[Transition, AllowAcquiredConversion, AllowRequiredConversion,
                 RequiredIter] = Val;
          auto &[Acquired, Function, Required] = Transition;
          if (AllowAcquiredConversion) {
            Acquired = DerivedTS;
          }
          if (AllowRequiredConversion) {
            Required.erase(RequiredIter);
            Required.emplace(DerivedTSValue);
          }
          return Transition;
        };

    auto NewTransitions = ranges::to_vector(
        Collector | ranges::views::transform(TransformToFilterData) |
        ranges::views::filter(HasTransitionWithBaseClass) |
        ranges::views::transform(FilterDataToNewTransition));
    spdlog::trace("adding new transitions for derived: {}", NewTransitions);
    Collector.insert(Collector.end(),
                     std::make_move_iterator(NewTransitions.begin()),
                     std::make_move_iterator(NewTransitions.end()));
    return true;
  }

  TransitionCollector &Collector;
};

void GetMe::HandleTranslationUnit(clang::ASTContext &Context) {
  // Traversing the translation unit decl via a RecursiveASTVisitor
  // will visit all nodes in the AST.
  GetMeVisitor Visitor{Collector};
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());

  auto PostVisitor = GetMePostVisitor{Collector};
  PostVisitor.TraverseDecl(Context.getTranslationUnitDecl());

  spdlog::trace("collected: {}", Visitor.Collector);
}

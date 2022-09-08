#include "get_me/tooling.hpp"

#include <functional>

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
ignoreFILEPredicate(const clang::FunctionDecl *FDecl) {
  if (FDecl->getReturnType().getUnqualifiedType().getAsString().find("FILE") !=
      std::string::npos) {
    return true;
  }
  return ranges::any_of(
      FDecl->parameters(), [](const clang::ParmVarDecl *const PVDecl) {
        return PVDecl->getType().getUnqualifiedType().getAsString().find(
                   "FILE") != std::string::npos;
      });
}

[[nodiscard]] static bool
reservedIntentifiersPredicate(const clang::FunctionDecl *FDecl) {
  if (FDecl->getDeclName().isIdentifier() && FDecl->getName().startswith("_")) {
    return true;
  }
  return FDecl->getReturnType().getUnqualifiedType().getAsString().starts_with(
      "_");
}

[[nodiscard]] static bool
requiredContainsAcquiredPredicate(const clang::FunctionDecl *FDecl) {
  return ranges::contains(FDecl->parameters(),
                          FDecl->getReturnType().getUnqualifiedType(),
                          [](const clang::ParmVarDecl *const PVarDecl) {
                            return PVarDecl->getType().getUnqualifiedType();
                          });
}

[[nodiscard]] static bool
functionFilterPredicate(const clang::FunctionDecl *FDecl,
                        TransitionCollector &Collector) {
  if (reservedIntentifiersPredicate(FDecl)) {
    return true;
  }
  if (ignoreFILEPredicate(FDecl)) {
    return true;
  }
  // FIXME: maybe need heuristic to reduce unwanted edges
  if (FDecl->getReturnType()->isArithmeticType()) {
    spdlog::trace("filtered due to returning arithmetic type: {}",
                  FDecl->getNameAsString());
    return true;
  }
  if (requiredContainsAcquiredPredicate(FDecl)) {
    spdlog::trace("filtered due to require-acquire cycle: {}",
                  FDecl->getNameAsString());
    return true;
  }
  // FIXME: support templates
  // if (FDecl->isTemplateDecl()) {
  //   return true;
  // }

  if (FDecl->getReturnType().getAsString().find("exception") !=
      std::string::npos) {
    return true;
  }

  if (ranges::any_of(
          FDecl->parameters(), [](const clang::ParmVarDecl *const PVDecl) {
            return PVDecl->getType().getAsString().find("exception") !=
                   std::string::npos;
          })) {
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

  [[nodiscard]] bool VisitFunctionDecl(clang::FunctionDecl *FDecl);

  [[nodiscard]] bool VisitFieldDecl(clang::FieldDecl *Field);

  [[nodiscard]] bool VisitCXXRecordDecl(clang::CXXRecordDecl *RDecl);

  [[nodiscard]] bool VisitVarDecl(clang::VarDecl *VDecl);

  TransitionCollector &Collector;
};

bool GetMeVisitor::VisitFunctionDecl(clang::FunctionDecl *FDecl) {
  if (llvm::isa<clang::CXXMethodDecl>(FDecl)) {
    return true;
  }
  if (functionFilterPredicate(FDecl, Collector)) {
    return true;
  }

  auto [Acquired, Required] = toTypeSet(FDecl);
  Collector.emplace_back(std::move(Acquired), TransitionDataType{FDecl},
                         std::move(Required));
  return true;
}

bool GetMeVisitor::VisitFieldDecl(clang::FieldDecl *Field) {
  if (Field->getName().startswith("_")) {
    return true;
  }

  if (Field->getAccess() != clang::AccessSpecifier::AS_public) {
    return true;
  }

  if (Field->getType()->isArithmeticType()) {
    return true;
  }
  if (Field->getType().getAsString().find("exception") != std::string::npos) {
    return true;
  }

  auto [Acquired, Required] = toTypeSet(Field);
  Collector.emplace_back(std::move(Acquired), TransitionDataType{Field},
                         std::move(Required));
  return true;
}

bool GetMeVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl *RDecl) {
  if (RDecl != RDecl->getDefinition()) {
    return true;
  }
  if (RDecl->getName().startswith("_")) {
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
    if (functionFilterPredicate(Method, Collector)) {
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

static void filterOverloads(std::vector<TransitionDataType> &Data,
                            size_t OverloadFilterParameterCountThreshold = 0) {
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
        Overloaded{
            GetNameOfDeclaratorDecl,
            [](const CustomTransitionType &CustomVal) { return CustomVal; },
            [](std::monostate) -> std::string { return "monostate"; }},
        Val);
  };
  const auto GetParameters = [](const TransitionDataType &Val) {
    return std::visit(
        Overloaded{
            [](const clang::FunctionDecl *const CurrentDecl)
                -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
              return CurrentDecl->parameters();
            },
            [](auto) -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
              return {};
            }},
        Val);
  };
  const auto Comparator =
      [&GetName, &GetParameters, OverloadFilterParameterCountThreshold](
          const TransitionDataType &Lhs, const TransitionDataType &Rhs) {
        if (const auto IndexComparison = Lhs.index() <=> Rhs.index();
            std::is_neq(IndexComparison)) {
          return std::is_lt(IndexComparison);
        }
        if (const auto NameComparison = GetName(Lhs) <=> GetName(Rhs);
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
  ranges::sort(Data, Comparator);
  const auto IsOverload = [&GetName,
                           &GetParameters](const TransitionDataType &Lhs,
                                           const TransitionDataType &Rhs) {
    if (Lhs.index() != Rhs.index()) {
      return false;
    }
    if (GetName(Lhs) != GetName(Rhs)) {
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

class GetMePostVisitor : public clang::RecursiveASTVisitor<GetMePostVisitor> {
public:
  explicit GetMePostVisitor(TransitionCollector &Collector)
      : Collector{Collector} {}

  [[nodiscard]] bool VisitCXXRecordDecl(clang::CXXRecordDecl *RDecl) {
    if (RDecl->isInvalidDecl()) {
      spdlog::trace("filtered due to being invalid decl: {}",
                    RDecl->getNameAsString());
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
    if (RDecl != RDecl->getDefinition()) {
      return true;
    }
    if (RDecl->getName().startswith("_")) {
      return true;
    }
    if (RDecl->getNameAsString().empty()) {
      spdlog::trace("filtered due to having empty name");
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

bool GetMeVisitor::VisitVarDecl(clang::VarDecl *VDecl) {
  if (VDecl->getNameAsString().starts_with("_")) {
    return true;
  }
  if (VDecl->isCXXInstanceMember()) {
    return true;
  }
  if (!VDecl->isStaticDataMember()) {
    return true;
  }
  if (VDecl->getType()->isArithmeticType()) {
    return true;
  }
  if (VDecl->getType().getAsString().find("exception") != std::string::npos) {
    return true;
  }

  if (const auto *const RDecl =
          llvm::dyn_cast<clang::RecordDecl>(VDecl->getDeclContext())) {
    Collector.emplace_back(
        TypeSet{
            TypeSetValueType{VDecl->getType().getCanonicalType().getTypePtr()}},
        TransitionDataType{VDecl}, TypeSet{});
  }
  return true;
}

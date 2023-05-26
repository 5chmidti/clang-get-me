#include "get_me/tooling.hpp"

#include <compare>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclObjC.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Redeclarable.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/StmtIterator.h>
#include <clang/AST/TemplateBase.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Sema/Sema.h>
#include <llvm/Support/Casting.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/action/unique.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/mismatch.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "get_me/propagate_inheritance.hpp"
#include "get_me/propagate_type_aliasing.hpp"
#include "get_me/tooling_filters.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/functional_clang.hpp"
#include "support/ranges/ranges.hpp"
#include "support/variant.hpp"

template <typename T>
[[nodiscard]] TransitionType toTransitionType(const T *const Transition,
                                              const Config &Conf) {

  auto [Acquired, Required] = toTypeSet(Transition, Conf);
  return {0U,
          {std::move(Acquired), TransitionDataType{Transition},
           std::move(Required)}};
}

namespace {
// FIXME: add support for current context (i.e. current function)
// this would mean only traversing into a function definition if it is the
// current context
class GetMe : public clang::ASTConsumer {
public:
  explicit GetMe(std::shared_ptr<Config> Conf,
                 std::shared_ptr<TransitionCollector> Transitions,
                 clang::Sema &Sema)
      : Conf_{std::move(Conf)},
        Transitions_{std::move(Transitions)},
        Sema_{Sema} {}

  void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
  std::shared_ptr<Config> Conf_;
  std::shared_ptr<TransitionCollector> Transitions_;
  clang::Sema &Sema_;
};

[[nodiscard]] auto getParametersOpt(const TransitionDataType &Val) {
  return std::visit(
      Overloaded{
          [](const clang::FunctionDecl *const CurrentDecl)
              -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
            return CurrentDecl->parameters();
          },
          [](auto &&) -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
            return {};
          }},
      Val);
}

void filterOverloads(TransitionCollector &Transitions,
                     const size_t OverloadFilterParameterCountThreshold = 0) {

  const auto Comparator =
      [OverloadFilterParameterCountThreshold](const TransitionDataType &Lhs,
                                              const TransitionDataType &Rhs) {
        if (const auto IndexComparison = Lhs.index() <=> Rhs.index();
            std::is_neq(IndexComparison)) {
          return std::is_lt(IndexComparison);
        }
        if (const auto NameComparison =
                getTransitionName(Lhs) <=> getTransitionName(Rhs);
            std::is_neq(NameComparison)) {
          return std::is_lt(NameComparison);
        }
        const auto LhsParams = getParametersOpt(Lhs);
        if (!LhsParams) {
          return true;
        }
        const auto RhsParams = getParametersOpt(Rhs);
        if (!RhsParams) {
          return false;
        }

        if (LhsParams->empty()) {
          return true;
        }
        if (RhsParams->empty()) {
          return false;
        }

        if (const auto MismatchResult =
                ranges::mismatch(LhsParams.value(), RhsParams.value(),
                                 std::equal_to{}, ToQualType, ToQualType);
            MismatchResult.in1 == LhsParams.value().end()) {
          return static_cast<size_t>(
                     std::distance(LhsParams->begin(), MismatchResult.in1)) <
                 OverloadFilterParameterCountThreshold;
        }
        return false;
      };
  const auto IsOverload = [](const StrippedTransitionType &LhsTuple,
                             const StrippedTransitionType &RhsTuple) {
    const auto &Lhs = ToTransition(LhsTuple);
    const auto &Rhs = ToTransition(RhsTuple);
    if (Lhs.index() != Rhs.index()) {
      return false;
    }
    if (getTransitionName(Lhs) != getTransitionName(Rhs)) {
      return false;
    }
    const auto LhsParams = getParametersOpt(Lhs);
    if (!LhsParams) {
      return false;
    }
    const auto RhsParams = getParametersOpt(Rhs);
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

  // FIXME: sorting just to make sure the overloads with longer parameter lists
  // are removed, figure out a better way. The algo also depends on this order
  // to determine if it is an overload
  Transitions.Data =
      Transitions.Data | ranges::views::move |
      ranges::views::transform([&IsOverload, &Comparator](
                                   BundeledTransitionType BundeledTransition) {
        return std::pair{ToAcquired(BundeledTransition),
                         std::move(BundeledTransition.second) |
                             ranges::actions::sort(Comparator, ToTransition) |
                             ranges::actions::unique(IsOverload) |
                             ranges::to<StrippedTransitionsSet>};
      }) |
      ranges::to<TransitionCollector::associative_container_type>;
}

} // namespace

class GetMeVisitor : public clang::RecursiveASTVisitor<GetMeVisitor> {
public:
  GetMeVisitor(std::shared_ptr<Config> Conf,
               TransitionCollector &TransitionsRef,
               std::vector<const clang::CXXRecordDecl *> &CXXRecordsRef,
               std::vector<TypeAlias> &TypedefNameDeclsRef,
               clang::Sema &SemaRef)
      : Conf_{std::move(Conf)},
        Transitions_{TransitionsRef},
        CxxRecords_{CXXRecordsRef},
        TypedefNameDecls_{TypedefNameDeclsRef},
        Sema_{SemaRef} {}

  [[nodiscard]] bool TraverseDecl(clang::Decl *Decl) {
    if (Decl == nullptr) {
      return true;
    }
    if (!Conf_->EnableFilterStd || !Decl->isInStdNamespace()) {
      clang::RecursiveASTVisitor<GetMeVisitor>::TraverseDecl(Decl);
    }
    return true;
  }

  [[nodiscard]] bool VisitFunctionDecl(clang::FunctionDecl *FDecl) {
    // handled differently via iterating over a CXXRecord's methods
    if (llvm::isa<clang::CXXMethodDecl>(FDecl)) {
      return true;
    }
    if (filterOut(FDecl, *Conf_)) {
      return true;
    }

    addTransition(toTransitionType(FDecl, *Conf_));
    return true;
  }

  [[nodiscard]] bool VisitFieldDecl(clang::FieldDecl *FDecl) {
    if (hasReservedIdentifierNameOrType(FDecl)) {
      return true;
    }

    // FIXME: filter access spec for members, depends on context of query
    if (FDecl->getAccess() != clang::AccessSpecifier::AS_public) {
      return true;
    }

    if (Conf_->EnableFilterArithmeticTransitions &&
        FDecl->getType()->isArithmeticType()) {
      return true;
    }

    if (hasTypeNameContainingName(FDecl, "exception")) {
      return true;
    }

    addTransition(toTransitionType(FDecl, *Conf_));
    return true;
  }

  [[nodiscard]] bool VisitCXXRecordDecl(clang::CXXRecordDecl *RDecl) {
    if (filterOut(RDecl, *Conf_)) {
      return true;
    }
    const auto *const Definition = [RDecl]() {
      if (RDecl->isThisDeclarationADefinition()) {
        return RDecl;
      }
      if (auto *const Instantiation =
              RDecl->getTemplateInstantiationPattern()) {
        return Instantiation;
      }
      return RDecl->getDefinition();
    }();

    if (Definition == nullptr) {
      return true;
    }

    // declare implicit default constructor even if it is not used
    if (Definition->needsImplicitDefaultConstructor()) {
      Sema_.DeclareImplicitDefaultConstructor(RDecl);
    }

    ranges::for_each(
        Definition->methods() |
            ranges::views::filter([this](const auto *const Function) {
              return !filterOut(Function, *Conf_);
            }),
        [this](const auto *const Function) {
          addTransition(toTransitionType(Function, *Conf_));
        });

    if (!ranges::empty(Definition->bases())) {
      CxxRecords_.push_back(RDecl);
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
    if (Conf_->EnableFilterStd && VDecl->isInStdNamespace()) {
      return true;
    }
    if (Conf_->EnableFilterArithmeticTransitions &&
        VDecl->getType()->isArithmeticType()) {
      return true;
    }
    if (hasReservedIdentifierNameOrType(VDecl)) {
      return true;
    }
    if (hasTypeNameContainingName(VDecl, "exception")) {
      return true;
    }

    Transitions_.Data[std::get<0>(toTypeSet(VDecl, *Conf_))].emplace(
        0U, std::pair{TransitionDataType{VDecl}, TypeSet{}});
    return true;
  }

  [[nodiscard]] bool VisitTypedefNameDecl(clang::TypedefNameDecl *NDecl) {
    if (NDecl->isInvalidDecl()) {
      return true;
    }
    if (Conf_->EnableFilterStd && NDecl->isInStdNamespace()) {
      return true;
    }
    if (NDecl->isTemplateDecl()) {
      return true;
    }
    const auto UnderlyingType = NDecl->getUnderlyingType();
    const auto AliasType =
        NDecl->getASTContext().getTypedefType(NDecl, UnderlyingType);
    if (UnderlyingType.getTypePtr() == nullptr) {
      GetMeException::fail(
          "unreachable, type alias should always have an underlying type");
      return true;
    }
    if (hasReservedIdentifierNameOrType(NDecl)) {
      return true;
    }

    TypedefNameDecls_.emplace_back(UnderlyingType, AliasType);
    return true;
  }

private:
  void addTransition(TransitionType Transition) {
    Transitions_.Data[ToAcquired(Transition)].emplace(
        0U, std::pair{ToTransition(Transition), ToRequired(Transition)});
  }

  std::shared_ptr<Config> Conf_;
  TransitionCollector &Transitions_;
  std::vector<const clang::CXXRecordDecl *> &CxxRecords_;
  std::vector<TypeAlias> &TypedefNameDecls_;
  clang::Sema &Sema_;
};

void GetMe::HandleTranslationUnit(clang::ASTContext &Context) {
  std::vector<const clang::CXXRecordDecl *> CXXRecords{};
  std::vector<TypeAlias> TypedefNameDecls{};
  GetMeVisitor Visitor{Conf_, *Transitions_, CXXRecords, TypedefNameDecls,
                       Sema_};

  std::ignore = Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  if (Conf_->EnableFilterOverloads) {
    filterOverloads(*Transitions_);
  }

  if (Conf_->EnablePropagateInheritance) {
    propagateInheritance(*Transitions_, CXXRecords);
  }
  if (Conf_->EnablePropagateTypeAlias) {
    propagateTypeAliasing(*Transitions_, TypedefNameDecls);
  }

  Transitions_->commit();
}

std::shared_ptr<TransitionCollector>
collectTransitions(clang::ASTUnit &AST, std::shared_ptr<Config> Conf) {
  auto Transitions = std::make_shared<TransitionCollector>();
  GetMe{std::move(Conf), Transitions, AST.getSema()}.HandleTranslationUnit(
      AST.getASTContext());
  return Transitions;
}

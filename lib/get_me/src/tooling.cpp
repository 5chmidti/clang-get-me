#include "get_me/tooling.hpp"

#include <memory>
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
#include <fmt/ranges.h>
#include <llvm/Support/Casting.h>
#include <range/v3/action/remove_if.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "get_me/propagate_inheritance.hpp"
#include "get_me/propagate_type_aliasing.hpp"
#include "get_me/propagate_type_conversions.hpp"
#include "get_me/tooling_filters.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/functional.hpp"
#include "support/ranges/ranges.hpp"
#include "support/variant.hpp"

namespace {
template <typename T>
[[nodiscard]] TransitionType toTransitionType(const T *const Transition,
                                              const Config &Conf) {

  auto [Acquired, Required] = toTypeSet(Transition, Conf);
  return {std::pair{std::move(Acquired), std::move(Required)},
          {0U, StrippedTransitionsSet{StrippedTransitionType{0U, Transition}}}};
}

// FIXME: add support for current context (i.e. current function)
// this would mean only traversing into a function definition if it is the
// current context
class GetMe : public clang::ASTConsumer {
public:
  explicit GetMe(std::shared_ptr<Config> Conf,
                 std::shared_ptr<TransitionData> Transitions, clang::Sema &Sema)
      : Conf_{std::move(Conf)},
        Transitions_{std::move(Transitions)},
        Sema_{Sema} {}

  void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
  std::shared_ptr<Config> Conf_;
  std::shared_ptr<TransitionData> Transitions_;
  clang::Sema &Sema_;
};
} // namespace

class GetMeVisitor : public clang::RecursiveASTVisitor<GetMeVisitor> {
public:
  GetMeVisitor(std::shared_ptr<Config> Conf, TransitionData &TransitionsRef,
               std::vector<const clang::CXXRecordDecl *> &CXXRecordsRef,
               std::vector<TransparentType> &TypedefNameDeclsRef,
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

    maybeAddTransition(toTransitionType(FDecl, *Conf_));
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

    maybeAddTransition(toTransitionType(FDecl, *Conf_));
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
          maybeAddTransition(toTransitionType(Function, *Conf_));
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

    Value(Transitions_.Data[std::pair{std::get<0>(toTypeSet(VDecl, *Conf_)),
                                      TypeSet{}}])
        .emplace(StrippedTransitionType{0U, TransitionDataType{VDecl}});
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
  void maybeAddTransition(TransitionType Transition) {
    if (ranges::contains(
            ToRequired(Transition) |
                ranges::views::transform(&TransparentType::Desugared),
            ToAcquired(Transition).Desugared)) {
      if (Conf_->EnableVerboseTransitionCollection) {
        spdlog::trace("addTransition: filtered out {} because the acquired is "
                      "contained in "
                      "required when using the unqualified desugared type",
                      Transition);
      }
      return;
    }
    const auto IsVoidOrVoidPtrType = [](const TransparentType &Val) {
      return std::visit(Overloaded{[](const clang::QualType &DesugardType) {
                                     return DesugardType->isVoidType() ||
                                            DesugardType->isVoidPointerType();
                                   },
                                   [](const auto &) { return false; }},
                        Val.Desugared);
    };

    auto Key = Index(Transition);
    auto &[Acquired, Required] = Key;
    if (IsVoidOrVoidPtrType(Acquired)) {
      return;
    }
    Value(Transitions_.Data[std::pair{
              Acquired, std::move(Required) |
                            ranges::actions::remove_if(IsVoidOrVoidPtrType)}])
        .merge(ToTransitions(std::move(Transition)));
  }

  std::shared_ptr<Config> Conf_;
  TransitionData &Transitions_;
  std::vector<const clang::CXXRecordDecl *> &CxxRecords_;
  std::vector<TransparentType> &TypedefNameDecls_;
  clang::Sema &Sema_;
};

void GetMe::HandleTranslationUnit(clang::ASTContext &Context) {
  std::vector<const clang::CXXRecordDecl *> CXXRecords{};
  std::vector<TransparentType> TypedefNameDecls{};
  GetMeVisitor Visitor{Conf_, *Transitions_, CXXRecords, TypedefNameDecls,
                       Sema_};

  std::ignore = Visitor.TraverseDecl(Context.getTranslationUnitDecl());

  if (Conf_->EnablePropagateInheritance) {
    propagateInheritance(*Transitions_, CXXRecords, *Conf_);
  }
  if (Conf_->EnablePropagateTypeAlias) {
    propagateTypeAliasing(Transitions_->ConversionMap, TypedefNameDecls);
  }

  propagateTypeConversions(*Transitions_);

  Transitions_->commit();
}

std::shared_ptr<TransitionData>
collectTransitions(clang::ASTUnit &AST, std::shared_ptr<Config> Conf) {
  auto Transitions = std::make_shared<TransitionData>();
  GetMe{std::move(Conf), Transitions, AST.getSema()}.HandleTranslationUnit(
      AST.getASTContext());
  return Transitions;
}

#include "get_me/tooling_filters.hpp"

#include <clang/AST/CanonicalType.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>

bool hasTypeNameContainingName(const clang::ValueDecl *const VDecl,
                               std::string_view Name) {
  return VDecl->getType().getAsString().find(Name) != std::string::npos;
}

bool hasReservedIdentifierName(const clang::QualType &QType) {
  auto QTypeAsString = QType.getAsString();
  return QTypeAsString.starts_with("_") ||
         (QTypeAsString.find("::_") != std::string::npos);
}

bool hasReservedIdentifierType(const clang::Type *const Type) {
  return hasReservedIdentifierName(clang::QualType(Type, 0));
}

bool isReturnTypeInParameterList(const clang::FunctionDecl *const FDecl) {
  return ranges::contains(
      FDecl->parameters(),
      FDecl->getReturnType()->getCanonicalTypeUnqualified(),
      [](const clang::ParmVarDecl *const PVarDecl) {
        return PVarDecl->getType()->getCanonicalTypeUnqualified();
      });
}

bool filterOut(const clang::FunctionDecl *const FDecl, const Config &Conf) {
  using namespace std::string_view_literals;

  if (FDecl->isDeleted()) {
    return true;
  }
  if (hasReservedIdentifierNameOrType(FDecl)) {
    return true;
  }
  if (hasAnyParameterOrReturnTypeWithName(
          FDecl, std::array{"FILE"sv, "exception"sv, "bad_array_new_length"sv,
                            "bad_alloc"sv, "traits"sv})) {
    return true;
  }
  if (Conf.EnableFilterStd && FDecl->isInStdNamespace()) {
    return true;
  }
  if (FDecl->getReturnType()->isArithmeticType()) {
    return true;
  }
  if (!llvm::isa<clang::CXXConstructorDecl>(FDecl) &&
      FDecl->getReturnType()->isVoidType()) {
    return true;
  }
  if (llvm::isa<clang::CXXDeductionGuideDecl>(FDecl)) {
    return true;
  }
  if (isReturnTypeInParameterList(FDecl)) {
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

bool filterOut(const clang::CXXMethodDecl *const Method, const Config &Conf) {
  if (Method->isCopyAssignmentOperator() ||
      Method->isMoveAssignmentOperator()) {
    return true;
  }
  // FIXME: allow conversions
  if (llvm::isa<clang::CXXConversionDecl>(Method)) {
    return true;
  }
  if (llvm::isa<clang::CXXDestructorDecl>(Method)) {
    return true;
  }

  if (const auto *const Constructor =
          llvm::dyn_cast<clang::CXXConstructorDecl>(Method);
      (Constructor != nullptr) && Constructor->isCopyOrMoveConstructor()) {
    return true;
  }

  return filterOut(static_cast<const clang::FunctionDecl *>(Method), Conf);
}

bool filterOut(const clang::CXXRecordDecl *const RDecl, const Config &Conf) {
  using namespace std::string_view_literals;

  if (RDecl != RDecl->getDefinition()) {
    return true;
  }
  if (hasReservedIdentifierNameOrType(RDecl)) {
    return true;
  }
  if (RDecl->getNameAsString().empty()) {
    return true;
  }
  if (Conf.EnableFilterStd && RDecl->isInStdNamespace()) {
    return true;
  }
  if (const auto Spec = RDecl->getTemplateSpecializationKind();
      Spec != clang::TSK_Undeclared &&
      Spec != clang::TSK_ImplicitInstantiation &&
      Spec != clang::TSK_ExplicitInstantiationDefinition &&
      Spec != clang::TSK_ExplicitInstantiationDeclaration) {
    return true;
  }
  if (RDecl->isTemplateDecl()) {
    return true;
  }
  if (RDecl->isTemplated()) {
    return true;
  }
  if (hasNameContainingAny(RDecl, std::array{"FILE"sv, "exception"sv,
                                             "bad_array_new_length"sv,
                                             "bad_alloc"sv, "traits"sv})) {
    return true;
  }

  return false;
}

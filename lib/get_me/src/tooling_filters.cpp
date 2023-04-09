#include "get_me/tooling_filters.hpp"

#include <array>
#include <string>
#include <string_view>

#include <clang/AST/CanonicalType.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <clang/Basic/Specifiers.h>
#include <llvm/Support/Casting.h>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/view/indirect.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "support/ranges/ranges.hpp"

bool hasTypeNameContainingName(const clang::ValueDecl *const VDecl,
                               const std::string_view Name) {
  return VDecl->getType().getAsString().find(Name) != std::string::npos;
}

bool hasReservedIdentifierName(const clang::QualType &QType) {
  const auto QTypeAsString = toString(QType.getUnqualifiedType());
  return QTypeAsString.starts_with("_") ||
         (QTypeAsString.find("::_") != std::string::npos);
}

bool hasReservedIdentifierType(const clang::QualType &QType) {
  return hasReservedIdentifierName(QType);
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
  if (Conf.EnableFilterArithmeticTransitions &&
      FDecl->getReturnType()->isArithmeticType()) {
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
    return true;
  }
  return false;
}

bool filterOut(const clang::CXXMethodDecl *const Method, const Config &Conf) {
  if (Method->isCopyAssignmentOperator() ||
      Method->isMoveAssignmentOperator()) {
    return true;
  }

  // FIXME: filter access spec for members, depends on context of query
  if (Method->getAccess() != clang::AccessSpecifier::AS_public) {
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
      Constructor != nullptr) {
    if (Constructor->isCopyOrMoveConstructor()) {
      return true;
    }

    // FIXME: allow dependent on context
    if (ranges::any_of(Method->getParent()->methods() | ranges::views::indirect,
                       &clang::CXXMethodDecl::isPure)) {
      return true;
    }
  }

  return filterOut(static_cast<const clang::FunctionDecl *>(Method), Conf);
}

bool filterOut(const clang::CXXRecordDecl *const RDecl, const Config &Conf) {
  using namespace std::string_view_literals;

  if (RDecl->getDefinition() == nullptr) {
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
  if (RDecl->isTemplateDecl()) {
    return true;
  }
  if (hasNameContainingAny(RDecl, std::array{"FILE"sv, "exception"sv,
                                             "bad_array_new_length"sv,
                                             "bad_alloc"sv, "traits"sv})) {
    return true;
  }

  return false;
}

#include "get_me/type_set.hpp"

#include <utility>

#include <boost/container/flat_set.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <llvm/Support/Casting.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/config.hpp"

namespace {
[[nodiscard]] const clang::Type *launderType(const clang::Type *const Type) {
  if (const auto *const TypeAlias = Type->getAs<clang::TypedefType>()) {
    return TypeAlias->getDecl()->getTypeForDecl();
  }

  if (const auto *const RDecl = Type->getAsCXXRecordDecl()) {
    return RDecl->getTypeForDecl();
  }
  return Type;
}

[[nodiscard]] TransparentType
getAcquiredType(const clang::FunctionDecl *const FDecl, const Config &Conf) {
  if (const auto *const Constructor =
          llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl);
      Constructor != nullptr) {
    const auto *const Decl = Constructor->getParent();
    return toTypeSetValueType(clang::QualType{Decl->getTypeForDecl(), 0},
                              FDecl->getASTContext(), Conf);
  }
  return toTypeSetValueType(FDecl->getReturnType(), FDecl->getASTContext(),
                            Conf);
}

[[nodiscard]] TypeSet getRequiredTypes(const clang::FunctionDecl *const FDecl,
                                       const Config &Conf) {
  const auto Parameters = FDecl->parameters();
  auto Res =
      Parameters |
      ranges::views::transform([Conf](const clang::ParmVarDecl *const PVDecl) {
        return toTypeSetValueType(PVDecl->getType(), PVDecl->getASTContext(),
                                  Conf);
      }) |
      ranges::to<TypeSet>;
  if (const auto *const Method = llvm::dyn_cast<clang::CXXMethodDecl>(FDecl)) {
    if (!llvm::isa<clang::CXXConstructorDecl>(Method) && !Method->isStatic()) {
      Res.emplace(toTypeSetValueType(
          clang::QualType{Method->getParent()->getTypeForDecl(), 0},
          FDecl->getASTContext(), Conf));
    }
  }
  return Res;
}

} // namespace

Type toTypeSetValue(const clang::QualType &QType, const Config &Conf) {
  if (Conf.EnableTruncateArithmetic && QType->isArithmeticType()) {
    return ArithmeticType{};
  }
  return QType;
}

TransparentType toTypeSetValueType(const clang::QualType &QType,
                                   const clang::ASTContext &Ctx,
                                   const Config &Conf) {
  return {.Desugared = toTypeSetValue(QType.getDesugaredType(Ctx), Conf),
          .Actual = toTypeSetValue(QType, Conf)};
}

clang::QualType launderType(const clang::QualType &QType) {
  return {launderType(QType.getTypePtr()), QType.getLocalFastQualifiers()};
}

std::pair<TransparentType, TypeSet>
toTypeSet(const clang::FunctionDecl *const FDecl, const Config &Conf) {
  return {getAcquiredType(FDecl, Conf), getRequiredTypes(FDecl, Conf)};
}

std::pair<TransparentType, TypeSet>
toTypeSet(const clang::FieldDecl *const FDecl, const Config &Conf) {
  const auto &Ctx = FDecl->getASTContext();
  return {toTypeSetValueType(FDecl->getType(), Ctx, Conf),
          TypeSet{{toTypeSetValueType(
              clang::QualType{FDecl->getParent()->getTypeForDecl(), 0}, Ctx,
              Conf)}}};
}

std::pair<TransparentType, TypeSet> toTypeSet(const clang::VarDecl *const VDecl,
                                              const Config &Conf) {
  return {{toTypeSetValueType(VDecl->getType(), VDecl->getASTContext(), Conf)},
          {}};
}

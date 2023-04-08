#include "get_me/type_set.hpp"

#include <utility>
#include <variant>

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
} // namespace

TypeSetValueType toTypeSetValueType(const clang::QualType &QType,
                                    const Config &Conf) {
  if (Conf.EnableTruncateArithmetic && QType->isArithmeticType()) {
    return TypeSetValueType{ArithmeticType{launderType(QType)}};
  }
  return TypeSetValueType{launderType(QType)};
}

clang::QualType launderType(const clang::QualType &QType) {
  return {launderType(QType.getTypePtr()), QType.getLocalFastQualifiers()};
}

std::pair<TypeSetValueType, TypeSet>
toTypeSet(const clang::FunctionDecl *const FDecl, const Config &Conf) {
  const auto AcquiredType = [FDecl, Conf]() {
    if (const auto *const Constructor =
            llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl);
        Constructor) {
      const auto *const Decl = Constructor->getParent();
      return TypeSetValueType{clang::QualType{Decl->getTypeForDecl(), 0}};
    }
    return toTypeSetValueType(FDecl->getReturnType(), Conf);
  }();
  const auto RequiredTypes = [FDecl, Conf]() {
    const auto Parameters = FDecl->parameters();
    auto Res = Parameters |
               ranges::views::transform(
                   [Conf](const clang::ParmVarDecl *const PVDecl) {
                     return toTypeSetValueType(PVDecl->getType(), Conf);
                   }) |
               ranges::to<TypeSet>;
    if (const auto *const Method =
            llvm::dyn_cast<clang::CXXMethodDecl>(FDecl)) {
      if (!llvm::isa<clang::CXXConstructorDecl>(Method) &&
          !Method->isStatic()) {
        Res.emplace(clang::QualType{Method->getParent()->getTypeForDecl(), 0});
      }
    }
    return Res;
  }();
  return {{AcquiredType}, RequiredTypes};
}

std::pair<TypeSetValueType, TypeSet>
toTypeSet(const clang::FieldDecl *const FDecl, const Config &Conf) {
  return {
      {{toTypeSetValueType(FDecl->getType(), Conf)}},
      {{toTypeSetValueType(
          clang::QualType{FDecl->getParent()->getTypeForDecl(), 0}, Conf)}}};
}

std::pair<TypeSetValueType, TypeSet>
toTypeSet(const clang::VarDecl *const VDecl, const Config &Conf) {
  return {{{toTypeSetValueType(VDecl->getType(), Conf)}}, {}};
}

bool isSubset(const TypeSet &Superset, const TypeSet &Subset) {
  if (Subset.size() > Superset.size()) {
    return false;
  }
  auto SupersetIter = Superset.begin();
  const auto SupersetEnd = Superset.end();
  for (const auto &SubsetVal : Subset) {
    for (; SupersetIter != SupersetEnd && *SupersetIter != SubsetVal;
         ++SupersetIter) {
    }
  }
  return SupersetIter != SupersetEnd;
}

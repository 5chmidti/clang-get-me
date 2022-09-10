#include "get_me/type_set.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <llvm/Support/Casting.h>
#include <range/v3/view/transform.hpp>

#include "get_me/formatting.hpp"

TypeSetValueType toTypeSetValueType(const clang::Type *const Type) {
  return TypeSetValueType{launderType(Type)};
}

const clang::Type *launderType(const clang::Type *Type) {
  if (const auto *const TypeAlias = Type->getAs<clang::TypedefType>()) {
    return TypeAlias->getDecl()->getTypeForDecl();
  }

  if (const auto *const RDecl = Type->getAsCXXRecordDecl()) {
    return RDecl->getTypeForDecl();
  }
  return Type;
}

std::pair<TypeSet, TypeSet> toTypeSet(const clang::FunctionDecl *FDecl) {
  const auto AcquiredType = [FDecl]() {
    if (const auto *const Constructor =
            llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl);
        Constructor) {
      const auto *const Decl = Constructor->getParent();
      return TypeSetValueType{Decl->getTypeForDecl()};
    }
    const auto RQType = FDecl->getReturnType();
    return toTypeSetValueType(RQType.getTypePtr());
  }();
  const auto RequiredTypes = [FDecl]() {
    const auto Parameters = FDecl->parameters();
    auto ParameterTypeRange =
        Parameters |
        ranges::views::transform([](const clang::ParmVarDecl *PVDecl) {
          const auto QType = PVDecl->getType();
          return toTypeSetValueType(QType.getTypePtr());
        });
    auto Res = TypeSet{std::make_move_iterator(ParameterTypeRange.begin()),
                       std::make_move_iterator(ParameterTypeRange.end())};
    if (const auto *const Method =
            llvm::dyn_cast<clang::CXXMethodDecl>(FDecl)) {
      if (!llvm::isa<clang::CXXConstructorDecl>(Method) &&
          !Method->isStatic()) {
        Res.emplace(Method->getParent()->getTypeForDecl());
      }
    }
    return Res;
  }();
  return {{AcquiredType}, RequiredTypes};
}

std::pair<TypeSet, TypeSet> toTypeSet(const clang::FieldDecl *FDecl) {
  return {{{toTypeSetValueType(FDecl->getType().getTypePtr())}},
          {{toTypeSetValueType(FDecl->getParent()->getTypeForDecl())}}};
}

std::pair<TypeSet, TypeSet> toTypeSet(const clang::VarDecl *VDecl) {
  return {{{toTypeSetValueType(VDecl->getType().getTypePtr())}}, {}};
}

#include "get_me/type_set.hpp"

#include <clang/AST/DeclCXX.h>
#include <range/v3/view/transform.hpp>

std::pair<TypeSet, TypeSet> toTypeSet(const clang::FunctionDecl *FDecl) {
  const auto AcquiredType = [FDecl]() {
    if (const auto *const Constructor =
            llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl);
        Constructor) {
      const auto *const Decl = Constructor->getParent();
      return TypeSetValueType{Decl->getTypeForDecl()};
    }
    const auto RQType = FDecl->getReturnType().getCanonicalType();
    const auto *const ReturnTypePtr = RQType.getTypePtr();
    return TypeSetValueType{ReturnTypePtr};
  }();
  const auto RequiredTypes = [FDecl]() {
    const auto Parameters = FDecl->parameters();
    auto ParameterTypeRange =
        Parameters |
        ranges::views::transform([](const clang::ParmVarDecl *PVDecl) {
          const auto QType = PVDecl->getType().getCanonicalType();
          return TypeSetValueType{QType.getTypePtr()};
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
  return {{{FDecl->getType().getCanonicalType().getTypePtr()}},
          {{FDecl->getParent()->getTypeForDecl()}}};
}

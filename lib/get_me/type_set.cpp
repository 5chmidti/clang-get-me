#include "get_me/type_set.hpp"

#include <iterator>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/Casting.h>
#include <range/v3/view/transform.hpp>

#include "get_me/config.hpp"

TypeSetValueType toTypeSetValueType(const clang::Type *const Type,
                                    const Config &Conf) {
  const auto *const ResultType = launderType(Type);
  if (Conf.EnableTruncateArithmetic && ResultType->isArithmeticType()) {
    return TypeSetValueType{ArithmeticType{ResultType}};
  }
  return TypeSetValueType{ResultType};
}

const clang::Type *launderType(const clang::Type *Type) {
  if (Type == nullptr) {
    return nullptr;
  }
  if (const auto *const TypeAlias = Type->getAs<clang::TypedefType>()) {
    return TypeAlias->getDecl()->getTypeForDecl();
  }

  if (const auto *const RDecl = Type->getAsCXXRecordDecl()) {
    return RDecl->getTypeForDecl();
  }
  return Type;
}

std::pair<TypeSet, TypeSet> toTypeSet(const clang::FunctionDecl *FDecl,
                                      const Config &Conf) {
  const auto AcquiredType = [FDecl, Conf]() {
    if (const auto *const Constructor =
            llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl);
        Constructor) {
      const auto *const Decl = Constructor->getParent();
      return TypeSetValueType{Decl->getTypeForDecl()};
    }
    const auto RQType = FDecl->getReturnType();
    return toTypeSetValueType(RQType.getTypePtr(), Conf);
  }();
  const auto RequiredTypes = [FDecl, Conf]() {
    const auto Parameters = FDecl->parameters();
    auto ParameterTypeRange =
        Parameters |
        ranges::views::transform([Conf](const clang::ParmVarDecl *PVDecl) {
          const auto QType = PVDecl->getType();
          return toTypeSetValueType(QType.getTypePtr(), Conf);
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

std::pair<TypeSet, TypeSet> toTypeSet(const clang::FieldDecl *FDecl,
                                      const Config &Conf) {
  return {{{toTypeSetValueType(FDecl->getType().getTypePtr(), Conf)}},
          {{toTypeSetValueType(FDecl->getParent()->getTypeForDecl(), Conf)}}};
}

std::pair<TypeSet, TypeSet> toTypeSet(const clang::VarDecl *VDecl,
                                      const Config &Conf) {
  return {{{toTypeSetValueType(VDecl->getType().getTypePtr(), Conf)}}, {}};
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

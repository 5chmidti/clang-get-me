#ifndef get_me_lib_get_me_include_get_me_tooling_filters_hpp
#define get_me_lib_get_me_include_get_me_tooling_filters_hpp

#include <string>
#include <string_view>
#include <type_traits>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <range/v3/algorithm/any_of.hpp>

#include "get_me/config.hpp"
#include "support/concepts.hpp"

namespace clang {
class CXXMethodDecl;
class CXXRecordDecl;
} // namespace clang

class Config;

[[nodiscard]] bool hasTypeNameContainingName(const clang::ValueDecl *VDecl,
                                             std::string_view Name);

[[nodiscard]] bool hasReservedIdentifierName(const clang::QualType &QType);

[[nodiscard]] bool hasReservedIdentifierType(const clang::Type *Type);

template <typename T>
[[nodiscard]] static bool
hasReservedIdentifierTypeOrReturnType(const T *const Decl)
  requires IsAnyOf<T, clang::FunctionDecl, clang::VarDecl, clang::FieldDecl,
                   clang::TypedefNameDecl> ||
           std::derived_from<T, clang::RecordDecl>
{
  const auto GetReturnTypeOrValueType = [Decl]() -> clang::QualType {
    if constexpr (std::is_same_v<T, clang::FunctionDecl>) {
      return Decl->getReturnType();
    } else if constexpr (std::is_same_v<T, clang::VarDecl> ||
                         std::is_same_v<T, clang::FieldDecl>) {
      return Decl->getType();
    } else if constexpr (std::is_same_v<T, clang::TypedefNameDecl> ||
                         std::derived_from<T, clang::RecordDecl>) {
      return clang::QualType(Decl->getTypeForDecl(), 0);
    }
  };
  return hasReservedIdentifierName(GetReturnTypeOrValueType());
}

template <typename T>
[[nodiscard]] static bool hasReservedIdentifierNameOrType(const T *const Decl)
  requires IsAnyOf<T, clang::FunctionDecl, clang::VarDecl, clang::FieldDecl,
                   clang::TypedefNameDecl> ||
           std::derived_from<T, clang::RecordDecl>
{
  if (Decl->getDeclName().isIdentifier() && Decl->getName().startswith("_")) {
    return true;
  }
  if (hasReservedIdentifierTypeOrReturnType(Decl)) {
    return true;
  }
  if constexpr (std::is_same_v<T, clang::TypedefNameDecl>) {
    return hasReservedIdentifierType(Decl->getUnderlyingType().getTypePtr());
  }
  return false;
}

[[nodiscard]] bool
isReturnTypeInParameterList(const clang::FunctionDecl *FDecl);

[[nodiscard]] static bool containsAny(const std::string &Str,
                                      ranges::range auto RangeOfNames) {
  return ranges::any_of(RangeOfNames, [&Str](const auto &Name) {
    return Str.find(Name) != std::string::npos;
  });
}

[[nodiscard]] static bool
hasAnyParameterOrReturnTypeWithName(const clang::FunctionDecl *const FDecl,
                                    ranges::forward_range auto RangeOfNames) {
  return containsAny(FDecl->getReturnType().getAsString(), RangeOfNames) ||
         ranges::any_of(
             FDecl->parameters(),
             [&RangeOfNames](const clang::ParmVarDecl *const PVDecl) {
               return containsAny(PVDecl->getType().getAsString(),
                                  RangeOfNames);
             });
}

[[nodiscard]] static bool
hasNameContainingAny(const clang::NamedDecl *const NDecl,
                     ranges::range auto RangeOfNames) {
  return containsAny(NDecl->getNameAsString(), RangeOfNames);
}

[[nodiscard]] bool filterOut(const clang::FunctionDecl *FDecl,
                             const Config &Conf);

[[nodiscard]] bool filterOut(const clang::CXXMethodDecl *Method,
                             const Config &Conf);

[[nodiscard]] bool filterOut(const clang::CXXRecordDecl *RDecl,
                             const Config &Conf);

#endif

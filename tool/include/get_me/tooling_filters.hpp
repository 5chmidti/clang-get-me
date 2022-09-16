#ifndef get_me_tooling_filters_hpp
#define get_me_tooling_filters_hpp

#include <string_view>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"

[[nodiscard]] bool hasTypeNameContainingName(const clang::ValueDecl *VDecl,
                                             std::string_view Name);

[[nodiscard]] bool hasReservedIdentifierName(const clang::QualType &QType);

[[nodiscard]] bool hasReservedIdentifierType(const clang::Type *Type);

template <typename T>
[[nodiscard]] static bool
hasReservedIdentifierTypeOrReturnType(const T *const Decl) {
  const auto GetReturnTypeOrValueType = [Decl]() -> clang::QualType {
    if constexpr (std::is_same_v<T, clang::FunctionDecl>) {
      return Decl->getReturnType();
    } else if constexpr (std::is_same_v<T, clang::VarDecl> ||
                         std::is_same_v<T, clang::FieldDecl>) {
      return Decl->getType();
    } else if constexpr (std::is_same_v<T, clang::TypedefNameDecl>) {
      return clang::QualType(Decl->getTypeForDecl(), 0);
    } else {
      static_assert(std::is_same_v<T, void>,
                    "hasReservedIdentifierType called with unsupported type");
    }
  };
  return hasReservedIdentifierName(GetReturnTypeOrValueType());
}

template <typename T>
[[nodiscard]] static bool hasReservedIdentifierNameOrType(const T *const Decl) {
  if (Decl->getDeclName().isIdentifier() && Decl->getName().startswith("_")) {
    return true;
  }
  if constexpr (std::is_same_v<T, clang::FunctionDecl> ||
                std::is_same_v<T, clang::VarDecl> ||
                std::is_same_v<T, clang::FieldDecl>) {
    return hasReservedIdentifierTypeOrReturnType(Decl);
  }
  if constexpr (std::is_same_v<T, clang::TypedefNameDecl>) {
    return hasReservedIdentifierTypeOrReturnType(Decl) ||
           hasReservedIdentifierType(Decl->getUnderlyingType().getTypePtr());
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

#ifndef get_me_type_set_hpp
#define get_me_type_set_hpp

#include <compare>
#include <set>
#include <variant>

#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>

#include "get_me/config.hpp"

struct ArithmeticType {
  const clang::Type *Value{};

  [[nodiscard]] friend bool operator==(const ArithmeticType & /*Lhs*/,
                                       const ArithmeticType & /*Rhs*/) {
    return true;
  }
  [[nodiscard]] friend bool operator<(const ArithmeticType & /*Lhs*/,
                                      const ArithmeticType & /*Rhs*/) {
    return false;
  }
};

using TypeSetValueType = std::variant<const clang::Type *, ArithmeticType>;
using TypeSet = std::set<TypeSetValueType>;

[[nodiscard]] std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FieldDecl *FDecl, Config Conf);

[[nodiscard]] std::pair<TypeSet, TypeSet> toTypeSet(const clang::VarDecl *VDecl,
                                                    Config Conf);

[[nodiscard]] std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FunctionDecl *FDecl, Config Conf);

[[nodiscard]] TypeSetValueType toTypeSetValueType(const clang::Type *Type,
                                                  Config Conf);

[[nodiscard]] const clang::Type *launderType(const clang::Type *Type);

#endif

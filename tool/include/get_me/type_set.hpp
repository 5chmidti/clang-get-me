#ifndef get_me_type_set_hpp
#define get_me_type_set_hpp

#include <compare>
#include <set>
#include <variant>

#include <boost/container/flat_set.hpp>
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
using TypeSet = boost::container::flat_set<TypeSetValueType>;

[[nodiscard]] std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FieldDecl *FDecl, const Config &Conf);

[[nodiscard]] std::pair<TypeSet, TypeSet> toTypeSet(const clang::VarDecl *VDecl,
                                                    const Config &Conf);

[[nodiscard]] std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FunctionDecl *FDecl, const Config &Conf);

[[nodiscard]] TypeSetValueType toTypeSetValueType(const clang::Type *Type,
                                                  const Config &Conf);

[[nodiscard]] const clang::Type *launderType(const clang::Type *Type);

[[nodiscard]] bool isSubset(const TypeSet &Superset, const TypeSet &Subset);

#endif

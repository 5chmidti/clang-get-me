#ifndef get_me_type_set_hpp
#define get_me_type_set_hpp

#include <set>

#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>

struct TypeValue {
  const clang::Type *Value{};

  [[nodiscard]] friend auto operator<=>(const TypeValue &Lhs,
                                        const TypeValue &Rhs) = default;
};

using TypeSetValueType = TypeValue;
using TypeSet = std::set<TypeSetValueType>;

[[nodiscard]] std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FieldDecl *FDecl);

[[nodiscard]] std::pair<TypeSet, TypeSet>
toTypeSet(const clang::VarDecl *VDecl);

[[nodiscard]] std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FunctionDecl *FDecl);

[[nodiscard]] TypeSetValueType toTypeSetValueType(const clang::Type *Type);

[[nodiscard]] const clang::Type *launderType(const clang::Type *Type);

#endif

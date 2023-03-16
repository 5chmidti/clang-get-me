#ifndef get_me_lib_get_me_include_get_me_type_set_hpp
#define get_me_lib_get_me_include_get_me_type_set_hpp

#include <utility>
#include <variant>

#include <boost/container/flat_set.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <fmt/core.h>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "support/variant.hpp"

namespace clang {
class FieldDecl;
class FunctionDecl;
class Type;
class VarDecl;
} // namespace clang

class Config;

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

template <> class fmt::formatter<ArithmeticType> {
public:
  [[nodiscard]] constexpr auto parse(format_parse_context &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const ArithmeticType & /*unused*/,
                            FormatContext &Ctx) const -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "arithmetic");
  }
};

using TypeSetValueType = std::variant<const clang::Type *, ArithmeticType>;

template <> class fmt::formatter<TypeSetValueType> {
public:
  [[nodiscard]] constexpr auto parse(format_parse_context &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const TypeSetValueType &Val,
                            FormatContext &Ctx) const -> decltype(Ctx.out()) {
    return fmt::format_to(
        Ctx.out(), "{}",
        std::visit(Overloaded{[](const clang::Type *const Type) {
                                return toString(Type);
                              },
                              [](const ArithmeticType &Arithmetic) {
                                return fmt::format("{}", Arithmetic);
                              }},
                   Val));
  }
};

using TypeSet = boost::container::flat_set<TypeSetValueType>;

[[nodiscard]] std::pair<TypeSetValueType, TypeSet>
toTypeSet(const clang::FieldDecl *FDecl, const Config &Conf);

[[nodiscard]] std::pair<TypeSetValueType, TypeSet>
toTypeSet(const clang::VarDecl *VDecl, const Config &Conf);

[[nodiscard]] std::pair<TypeSetValueType, TypeSet>
toTypeSet(const clang::FunctionDecl *FDecl, const Config &Conf);

[[nodiscard]] TypeSetValueType toTypeSetValueType(const clang::Type *Type,
                                                  const Config &Conf);

[[nodiscard]] const clang::Type *launderType(const clang::Type *Type);

[[nodiscard]] bool isSubset(const TypeSet &Superset, const TypeSet &Subset);

#endif

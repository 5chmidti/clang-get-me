#ifndef get_me_lib_get_me_include_get_me_type_set_hpp
#define get_me_lib_get_me_include_get_me_type_set_hpp

#include <concepts>
#include <type_traits>
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
  clang::QualType Value{};

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

[[nodiscard]] clang::QualType launderType(const clang::QualType &Type);

class TypeSetValueType : public std::variant<clang::QualType, ArithmeticType> {
public:
  using Base = std::variant<clang::QualType, ArithmeticType>;
  using Base::variant;

  // force launder of all QualTypes for TypeSetValueType
  template <typename T>
    requires std::same_as<clang::QualType, std::remove_cvref_t<T>>
  // NOLINTBEGIN(bugprone-forwarding-reference-overload,
  // google-explicit-constructor, hicpp-explicit-conversions)
  explicit(false) TypeSetValueType(T &&QType)
      : Base{launderType(std::forward<T>(QType))} {}
  // NOLINTEND
};

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
        std::visit([](const auto &Val) { return fmt::format("{}", Val); },
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

[[nodiscard]] TypeSetValueType toTypeSetValueType(const clang::QualType &QType,
                                                  const Config &Conf);

[[nodiscard]] bool isSubset(const TypeSet &Superset, const TypeSet &Subset);

#endif

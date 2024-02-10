#ifndef get_me_lib_get_me_include_get_me_type_set_hpp
#define get_me_lib_get_me_include_get_me_type_set_hpp

#include <compare>
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

namespace clang {
class FieldDecl;
class FunctionDecl;
class Type;
class VarDecl;
} // namespace clang

class Config;

struct ArithmeticType {
  [[nodiscard]] friend auto
  operator<=>(const ArithmeticType &,
              const ArithmeticType &) noexcept = default;
};

template <> class fmt::formatter<ArithmeticType> {
public:
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &Ctx) {
    return Ctx.begin();
  }

  [[nodiscard]] format_context::iterator
  format(const ArithmeticType & /*unused*/, format_context &Ctx) const {
    return fmt::format_to(Ctx.out(), "arithmetic");
  }
};

[[nodiscard]] clang::QualType launderType(const clang::QualType &Type);

class TypeSetValue : public std::variant<clang::QualType, ArithmeticType> {
public:
  using Base = std::variant<clang::QualType, ArithmeticType>;
  using Base::variant;

  // force launder of all QualTypes for TypeSetValue
  template <typename T>
    requires std::same_as<clang::QualType, std::remove_cvref_t<T>>
  // NOLINTBEGIN(bugprone-forwarding-reference-overload,
  // google-explicit-constructor, hicpp-explicit-conversions)
  explicit(false) TypeSetValue(T &&QType)
      : Base{launderType(std::forward<T>(QType))} {}
  // NOLINTEND

  [[nodiscard]] friend auto operator<=>(const TypeSetValue &Lhs,
                                        const TypeSetValue &Rhs) {

    if (const auto Cmp = Lhs.index() <=> Rhs.index();
        Cmp != std::strong_ordering::equal) {
      return Cmp;
    }
    if (std::holds_alternative<ArithmeticType>(Lhs)) {
      return std::strong_ordering::equal;
    }

    const auto LQT = std::get<clang::QualType>(Lhs);
    const auto RQT = std::get<clang::QualType>(Rhs);
    if (const auto Cmp = LQT.getTypePtr() <=> RQT.getTypePtr();
        Cmp != std::strong_ordering::equal) {
      return Cmp;
    }

    return LQT.getCVRQualifiers() <=> RQT.getCVRQualifiers();
  }
};

template <> class fmt::formatter<TypeSetValue> {
public:
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &Ctx) {
    return Ctx.begin();
  }

  [[nodiscard]] format_context::iterator format(const TypeSetValue &Val,
                                                format_context &Ctx) const {
    return fmt::format_to(
        Ctx.out(), "{}",
        std::visit([](const auto &Value) { return fmt::format("{}", Value); },
                   Val));
  }
};

struct TypeSetValueType {
  TypeSetValue Desugared;
  TypeSetValue Actual;

  [[nodiscard]] friend auto operator<=>(const TypeSetValueType &Lhs,
                                        const TypeSetValueType &Rhs) = default;
};

struct TypeSetValueTypeLessActual {
  [[nodiscard]] static bool operator()(const TypeSetValueType &Lhs,
                                       const TypeSetValueType &Rhs) {
    return Lhs.Actual < Rhs.Actual;
  }
};
struct TypeSetValueTypeLessDesugared {
  [[nodiscard]] static bool operator()(const TypeSetValueType &Lhs,
                                       const TypeSetValueType &Rhs) {
    return Lhs.Desugared < Rhs.Desugared;
  }
};

template <> class fmt::formatter<TypeSetValueType> {
public:
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &Ctx) {
    const auto *Iter = Ctx.begin();
    const auto *const End = Ctx.end();
    if (Iter != End && (*Iter == 'a')) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      Presentation_ = *Iter++;
      Ctx.advance_to(Iter);
    }
    return Iter;
  }

  [[nodiscard]] format_context::iterator format(const TypeSetValueType &Val,
                                                format_context &Ctx) const {
    if (Presentation_ != 'a') {
      return fmt::format_to(Ctx.out(), "{}", Val.Actual);
    }
    return fmt::format_to(Ctx.out(), "({}, {})", Val.Actual, Val.Desugared);
  }

private:
  char Presentation_{};
};

// FIXME: figure out if comparing with Desugared would be possible, and simplify
// other logic
using TypeSet = boost::container::flat_set<TypeSetValueType>;

[[nodiscard]] std::pair<TypeSetValueType, TypeSet>
toTypeSet(const clang::FieldDecl *FDecl, const Config &Conf);

[[nodiscard]] std::pair<TypeSetValueType, TypeSet>
toTypeSet(const clang::VarDecl *VDecl, const Config &Conf);

[[nodiscard]] std::pair<TypeSetValueType, TypeSet>
toTypeSet(const clang::FunctionDecl *FDecl, const Config &Conf);

[[nodiscard]] TypeSetValue toTypeSetValue(const clang::QualType &QType,
                                          const Config &Conf);
[[nodiscard]] TypeSetValueType toTypeSetValueType(const clang::QualType &QType,
                                                  const clang::ASTContext &Ctx,
                                                  const Config &Conf);

[[nodiscard]] bool isSubset(const TypeSet &Superset, const TypeSet &Subset);

#endif

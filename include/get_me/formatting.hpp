#ifndef get_me_include_get_me_formatting_hpp
#define get_me_include_get_me_formatting_hpp

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include "get_me/graph.hpp"
#include "get_me/path_traversal.hpp"
#include "get_me/type_set.hpp"
#include "support/variant.hpp"

namespace clang {
class Type;
} // namespace clang

[[nodiscard]] std::string getTransitionName(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionAcquiredTypeNames(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionRequiredTypeNames(const TransitionDataType &Data);

[[nodiscard]] std::string toString(const clang::Type *Type);
[[nodiscard]] std::string toString(const clang::NamedDecl *NDecl);

template <> struct fmt::formatter<EdgeDescriptor> {
  template <typename FormatContext>
  [[nodiscard]] constexpr auto parse(FormatContext &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const EdgeDescriptor &Edge,
                            FormatContext &Ctx) const -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "{}",
                          std::pair{Source(Edge), Target(Edge)});
  }
};

template <> struct fmt::formatter<TransitionDataType> {
  template <typename FormatContext>
  [[nodiscard]] constexpr auto parse(FormatContext &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const TransitionDataType &Val,
                            FormatContext &Ctx) const -> decltype(Ctx.out()) {
    return fmt::format_to(
        Ctx.out(), "{} {}({})", getTransitionAcquiredTypeNames(Val),
        getTransitionName(Val), getTransitionRequiredTypeNames(Val));
  }
};

template <> struct fmt::formatter<TypeSetValueType> {
  template <typename FormatContext>
  [[nodiscard]] constexpr auto parse(FormatContext &Ctx)
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
                              [](const ArithmeticType & /*Arithmetic*/)
                                  -> std::string { return "arithmetic"; }},
                   Val));
  }
};

[[nodiscard]] std::vector<std::string>
toString(const std::vector<PathType> &Paths, const GraphType &Graph,
         const GraphData &Data);

#endif

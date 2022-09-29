#ifndef get_me_formatting_hpp
#define get_me_formatting_hpp

#include <string>
#include <utility>
#include <variant>

#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include "get_me/graph.hpp"
#include "get_me/type_set.hpp"
#include "get_me/utility.hpp"

[[nodiscard]] std::string getTransitionName(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionAcquiredTypeNames(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionRequiredTypeNames(const TransitionDataType &Data);

[[nodiscard]] std::string toString(const TransitionType &Transition);
[[nodiscard]] std::string toString(const clang::Type *Type);

template <> struct fmt::formatter<EdgeDescriptor> {
  template <typename FormatContext>
  [[nodiscard]] constexpr auto parse(FormatContext &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const EdgeDescriptor &Val, FormatContext &Ctx) const
      -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "{}",
                          std::pair{Val.m_source, Val.m_target});
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

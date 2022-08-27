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
#include <spdlog/spdlog.h>

#include "get_me/graph.hpp"
#include "get_me/utility.hpp"

[[nodiscard]] std::string getTransitionName(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionTargetTypeName(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionSourceTypeName(const TransitionDataType &Data);

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
        Ctx.out(), "{} {}({})", getTransitionTargetTypeName(Val),
        getTransitionName(Val), getTransitionSourceTypeName(Val));
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
        std::visit(
            Overloaded{
                [](clang::QualType QType) { return QType.getAsString(); },
                [](const clang::NamedDecl *NDecl) {
                  return NDecl->getNameAsString();
                },
                [](std::monostate) -> std::string { return "monostate"; }},
            Val.MetaValue));
  }
};

#endif

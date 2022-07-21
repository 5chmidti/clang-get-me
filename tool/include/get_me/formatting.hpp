#ifndef get_me_formatting_hpp
#define get_me_formatting_hpp

#include <string>
#include <variant>

#include <clang/AST/Decl.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include "get_me/graph_types.hpp"
#include "get_me/utility.hpp"

[[nodiscard]] std::string getTransitionName(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionTargetTypeName(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionSourceTypeName(const TransitionDataType &Data);

template <> struct fmt::formatter<TransitionDataType> {
  template <typename FormatContext>
  [[nodiscard]] constexpr auto parse(FormatContext &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const TransitionDataType &Val, FormatContext &Ctx)
      -> decltype(Ctx.out()) {
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
  [[nodiscard]] auto format(const TypeSetValueType &Val, FormatContext &Ctx)
      -> decltype(Ctx.out()) {
    return fmt::format_to(
        Ctx.out(), "{}",
        std::visit(Overloaded{[](const clang::QualType *QType) {
                                return QType->getAsString();
                              },
                              [](clang::QualType QType) {
                                return QType.getAsString();
                              },
                              [](const clang::NamedDecl *NDecl) {
                                return NDecl->getNameAsString();
                              }},
                   Val));
  }
};

#endif

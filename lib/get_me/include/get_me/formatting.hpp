#ifndef get_me_lib_get_me_include_get_me_formatting_hpp
#define get_me_lib_get_me_include_get_me_formatting_hpp

#include <string>

#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <fmt/core.h>

namespace clang {
class Type;
class Decl;
class NamedDecl;
} // namespace clang

namespace detail {
[[nodiscard]] inline clang::PrintingPolicy
normalize(clang::PrintingPolicy PrintingPolicy) {
  PrintingPolicy.SuppressTagKeyword = 1;
  return PrintingPolicy;
}
} // namespace detail

template <> class fmt::formatter<clang::QualType> {
public:
  [[nodiscard]] constexpr auto parse(format_parse_context &ctx)
      -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const clang::QualType &Val,
                            FormatContext &Ctx) const -> decltype(Ctx.out()) {
    static const auto Policy =
        ::detail::normalize(clang::PrintingPolicy{clang::LangOptions{}});
    return fmt::format_to(Ctx.out(), "{}", Val.getAsString(Policy));
  }
};

[[nodiscard]] std::string toString(const clang::NamedDecl *NDecl);

#endif

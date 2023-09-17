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
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &Ctx) {
    return Ctx.begin();
  }

  [[nodiscard]] format_context::iterator format(const clang::QualType &Val,
                                                format_context &Ctx) const {
    static const auto Policy =
        ::detail::normalize(clang::PrintingPolicy{clang::LangOptions{}});
    return fmt::format_to(Ctx.out(), "{}", Val.getAsString(Policy));
  }
};

[[nodiscard]] std::string toString(const clang::NamedDecl *NDecl);

#endif

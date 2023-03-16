#ifndef get_me_lib_get_me_include_get_me_propagate_type_aliasing_hpp
#define get_me_lib_get_me_include_get_me_propagate_type_aliasing_hpp

#include <vector>

#include <clang/AST/Decl.h>

#include "get_me/transitions.hpp"

struct TypeAlias {
  const clang::Type *Base;
  const clang::Type *Alias;
};

template <> class fmt::formatter<TypeAlias> {
public:
  [[nodiscard]] constexpr auto parse(format_parse_context &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const TypeAlias &Val, FormatContext &Ctx) const
      -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "({}, {})", toString(Val.Base),
                          toString(Val.Alias));
  }
};

void propagateTypeAliasing(TransitionCollector &Transitions,
                           const std::vector<TypeAlias> &TypedefNameDecls);

#endif

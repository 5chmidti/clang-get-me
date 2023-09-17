#ifndef get_me_lib_get_me_include_get_me_propagate_type_aliasing_hpp
#define get_me_lib_get_me_include_get_me_propagate_type_aliasing_hpp

#include <vector>

#include <clang/AST/Type.h>
#include <fmt/core.h>

#include "get_me/formatting.hpp"
#include "get_me/transitions.hpp"

struct TypeAlias {
  clang::QualType Base;
  clang::QualType Alias;
};

template <> class fmt::formatter<TypeAlias> {
public:
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &Ctx) {
    return Ctx.begin();
  }

  [[nodiscard]] format_context::iterator format(const TypeAlias &Val,
                                                format_context &Ctx) const {
    return fmt::format_to(Ctx.out(), "({}, {})", Val.Base, Val.Alias);
  }
};

void propagateTypeAliasing(TransitionData &Transitions,
                           const std::vector<TypeAlias> &TypedefNameDecls);

#endif

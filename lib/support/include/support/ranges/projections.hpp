#ifndef get_me_lib_support_include_support_projections_hpp
#define get_me_lib_support_include_support_projections_hpp

#include <cstddef>
#include <utility>

#include <clang/AST/Decl.h>

template <std::size_t I>
inline constexpr auto Element = []<typename T>(T &&Tuple) -> decltype(auto) {
  return std::get<I>(std::forward<T>(Tuple));
};

inline constexpr auto ToQualType = [](const clang::ValueDecl *const VDecl) {
  return VDecl->getType();
};

#endif

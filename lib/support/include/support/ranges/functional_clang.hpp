#ifndef get_me_lib_support_include_support_ranges_functional_clang_hpp
#define get_me_lib_support_include_support_ranges_functional_clang_hpp

#include <clang/AST/Decl.h>

inline constexpr auto ToQualType = [](const clang::ValueDecl *const VDecl) {
  return VDecl->getType();
};

#endif

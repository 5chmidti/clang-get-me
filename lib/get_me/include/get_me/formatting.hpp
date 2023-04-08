#ifndef get_me_lib_get_me_include_get_me_formatting_hpp
#define get_me_lib_get_me_include_get_me_formatting_hpp

#include <string>

#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>

namespace clang {
class Type;
class Decl;
class NamedDecl;
} // namespace clang

[[nodiscard]] std::string toString(const clang::QualType &QType);
[[nodiscard]] std::string toString(const clang::NamedDecl *NDecl);

#endif

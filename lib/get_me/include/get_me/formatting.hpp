#ifndef get_me_lib_get_me_include_get_me_formatting_hpp
#define get_me_lib_get_me_include_get_me_formatting_hpp

#include <string>

#include <clang/AST/PrettyPrinter.h>

namespace clang {
class Type;
class Decl;
class NamedDecl;
} // namespace clang

[[nodiscard]] clang::PrintingPolicy
getNormalizedPrintingPolicy(const clang::Decl *Decl);

[[nodiscard]] std::string toString(const clang::Type *Type);
[[nodiscard]] std::string toString(const clang::NamedDecl *NDecl);

#endif

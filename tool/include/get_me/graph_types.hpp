#ifndef get_me_graph_types_hpp
#define get_me_graph_types_hpp

#include <variant>

#include <clang/AST/Type.h>

namespace clang {
struct FunctionDecl;
struct FieldDecl;
} // namespace clang

using TransitionDataType =
    std::variant<clang::FunctionDecl *, clang::FieldDecl *>;
using TransitionSourceType = clang::Type;

#endif

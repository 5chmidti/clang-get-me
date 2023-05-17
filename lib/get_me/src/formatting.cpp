#include "get_me/formatting.hpp"

#include <clang/AST/Decl.h>

std::string toString(const clang::NamedDecl *const NDecl) {
  return NDecl->getNameAsString();
}

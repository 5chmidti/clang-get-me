#include "get_me/formatting.hpp"

#include <string>

#include <clang/AST/Decl.h>

std::string toString(const clang::NamedDecl *const NDecl) {
  return NDecl->getNameAsString();
}

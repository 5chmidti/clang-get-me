#include "get_me/formatting.hpp"

#include <string>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LangOptions.h>
#include <fmt/core.h>

namespace {
[[nodiscard]] clang::PrintingPolicy
normalize(clang::PrintingPolicy PrintingPolicy) {
  PrintingPolicy.SuppressTagKeyword = 1;
  return PrintingPolicy;
}
} // namespace

std::string toString(const clang::QualType &QType) {
  static const auto Policy =
      normalize(clang::PrintingPolicy{clang::LangOptions{}});
  return QType.getAsString(Policy);
}

std::string toString(const clang::NamedDecl *const NDecl) {
  return NDecl->getNameAsString();
}

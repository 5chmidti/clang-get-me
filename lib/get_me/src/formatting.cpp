#include "get_me/formatting.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/Type.h>

namespace {
[[nodiscard]] clang::PrintingPolicy
normalize(clang::PrintingPolicy PrintingPolicy) {
  PrintingPolicy.SuppressTagKeyword = 1;
  return PrintingPolicy;
}

} // namespace

clang::PrintingPolicy
getNormalizedPrintingPolicy(const clang::Decl *const Decl) {
  return normalize(Decl->getASTContext().getPrintingPolicy());
}

std::string toString(const clang::Type *const Type) {
  if (const auto *const Decl = Type->getAsTagDecl(); Decl != nullptr) {
    return clang::QualType(Type, 0).getAsString(
        getNormalizedPrintingPolicy(Decl));
  }
  return clang::QualType(Type, 0).getAsString();
}

std::string toString(const clang::NamedDecl *const NDecl) {
  return NDecl->getNameAsString();
}

#include "get_me/formatting.hpp"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <range/v3/view/transform.hpp>

#include "get_me/utility.hpp"

using namespace clang;

std::string getTransitionName(const TransitionDataType &Data) {
  return std::visit(Overloaded{[](const DeclaratorDecl *DDecl) {
                      return DDecl->getNameAsString();
                    }},
                    Data);
}

std::string getTransitionTargetTypeName(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{[](const FunctionDecl *FDecl) {
                   if (const auto *const Constructor =
                           dyn_cast<CXXConstructorDecl>(FDecl)) {
                     return Constructor->getParent()->getNameAsString();
                   }
                   return FDecl->getReturnType().getAsString();
                 },
                 [](const FieldDecl *FDecl) {
                   return FDecl->getType().getAsString();
                 }},
      Data);
}

std::string getTransitionSourceTypeName(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{
          [](const FunctionDecl *FDecl) {
            const auto Parameters = FDecl->parameters();
            return fmt::format(
                "{}", fmt::join(Parameters |
                                    ranges::views::transform(
                                        [](const ParmVarDecl *PDecl) {
                                          return PDecl->getType().getAsString();
                                        }),
                                ", "));
          },
          [](const FieldDecl *FDecl) {
            return FDecl->getParent()->getNameAsString();
          }},
      Data);
}

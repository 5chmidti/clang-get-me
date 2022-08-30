#include "get_me/formatting.hpp"

#include <variant>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <fmt/format.h>
#include <llvm/Support/Casting.h>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/utility.hpp"

using namespace clang;

std::string getTransitionName(const TransitionDataType &Data) {
  return std::visit(Overloaded{[](const DeclaratorDecl *DDecl) -> std::string {
                                 return DDecl->getNameAsString();
                               },
                               [](const std::monostate) -> std::string {
                                 return "monostate";
                               }},
                    Data);
}

[[nodiscard]] static std::string
functionDeclToStringForTarget(const FunctionDecl *FDecl) {
  if (const auto *const Constructor = dyn_cast<CXXConstructorDecl>(FDecl)) {
    const auto *const Parent = Constructor->getParent();
    return Parent->getNameAsString();
  }
  if (llvm::isa<CXXDestructorDecl>(FDecl)) {
    constexpr const auto *const Msg = "destructor";
    spdlog::error("functionDeclToStringForTarget {}", Msg);
    return Msg;
  }
  return FDecl->getReturnType().getAsString();
}

std::string getTransitionTargetTypeName(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{[](const FunctionDecl *FDecl) -> std::string {
                   return functionDeclToStringForTarget(FDecl);
                 },
                 [](const FieldDecl *FDecl) -> std::string {
                   return FDecl->getType().getAsString();
                 },
                 [](std::monostate) -> std::string { return "monostate"; }},
      Data);
}

std::string getTransitionSourceTypeName(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{
          [](const FunctionDecl *FDecl) -> std::string {
            const auto Parameters = FDecl->parameters();
            auto Params = fmt::format(
                "{}", fmt::join(Parameters |
                                    ranges::views::transform(
                                        [](const ParmVarDecl *PDecl) {
                                          return PDecl->getType().getAsString();
                                        }),
                                ", "));
            if (const auto *const Method =
                    llvm::dyn_cast<clang::CXXMethodDecl>(FDecl)) {
              const auto *const RDecl = Method->getParent();
              if (Params.empty()) {
                return RDecl->getNameAsString();
              }
              return fmt::format("{}, {}", RDecl->getNameAsString(), Params);
            }
            return Params;
          },
          [](const FieldDecl *FDecl) -> std::string {
            return FDecl->getParent()->getNameAsString();
          },
          [](const std::monostate) -> std::string { return "monostate"; }},
      Data);
}

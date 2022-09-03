#include "get_me/formatting.hpp"

#include <variant>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <fmt/format.h>
#include <llvm/Support/Casting.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/utility.hpp"

using namespace clang;

std::string getTransitionName(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{
          [](const DeclaratorDecl *DDecl) { return DDecl->getNameAsString(); },
          [](const std::monostate) -> std::string { return "monostate"; }},
      Data);
}

[[nodiscard]] static std::string
functionDeclToStringForAcquired(const FunctionDecl *const FDecl) {
  if (const auto *const Constructor = dyn_cast<CXXConstructorDecl>(FDecl)) {
    const auto *const Parent = Constructor->getParent();
    return Parent->getNameAsString();
  }
  return FDecl->getReturnType().getAsString();
}

std::string getTransitionAcquiredTypeNames(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{[](const FunctionDecl *const FDecl) {
                   return functionDeclToStringForAcquired(FDecl);
                 },
                 [](const FieldDecl *const FDecl) {
                   return FDecl->getType().getAsString();
                 },
                 [](std::monostate) -> std::string { return "monostate"; }},
      Data);
}

[[nodiscard]] static std::string
functionDeclToStringForRequired(const FunctionDecl *const FDecl) {
  const auto Parameters = FDecl->parameters();
  auto Params = fmt::format(
      "{}", fmt::join(Parameters | ranges::views::transform(
                                       [](const ParmVarDecl *PDecl) {
                                         return PDecl->getType().getAsString();
                                       }),
                      ", "));
  if (const auto *const Method = llvm::dyn_cast<CXXMethodDecl>(FDecl)) {
    if (!llvm::isa<CXXConstructorDecl>(Method)) {
      const auto *const RDecl = Method->getParent();
      if (Params.empty()) {
        return RDecl->getNameAsString();
      }
      return fmt::format("{}, {}", RDecl->getNameAsString(), Params);
    }
  }
  return Params;
};

std::string getTransitionRequiredTypeNames(const TransitionDataType &Data) {
  return std::visit(Overloaded{[](const FunctionDecl *const FDecl) {
                                 return functionDeclToStringForRequired(FDecl);
                               },
                               [](const FieldDecl *const FDecl) {
                                 return FDecl->getParent()->getNameAsString();
                               },
                               [](const std::monostate) -> std::string {
                                 return "monostate";
                               }},
                    Data);
}

std::vector<std::string> toString(const std::vector<PathType> &Paths,
                                  const GraphType &Graph,
                                  const GraphData &Data) {
  return ranges::to_vector(
      Paths | ranges::views::transform([&Graph, &Data](const auto &Path) {
        return fmt::format(
            "{}",
            fmt::join(
                Path |
                    ranges::views::transform(
                        [&Data,
                         IndexMap = boost::get(boost::edge_index, Graph)](
                            const EdgeDescriptor &Edge) {
                          return Data.EdgeWeights[boost::get(IndexMap, Edge)];
                                       }),
                            ", "));
      }));
};

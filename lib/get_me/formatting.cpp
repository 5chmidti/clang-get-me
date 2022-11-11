#include "get_me/formatting.hpp"

#include <algorithm>
#include <variant>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclarationName.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LLVM.h>
#include <fmt/format.h>
#include <llvm/Support/Casting.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/graph.hpp"
#include "get_me/utility.hpp"

using namespace clang;

std::string getTransitionName(const TransitionDataType &Data) {
  const auto DeclaratorDeclToString = [](const DeclaratorDecl *DDecl) {
    if (!DDecl->getDeclName().isIdentifier()) {
      if (const auto *const Constructor =
              llvm::dyn_cast<clang::CXXConstructorDecl>(DDecl)) {
        return Constructor->getParent()->getNameAsString();
      }
      return fmt::format("non-identifier {}({})", DDecl->getDeclKindName(),
                         static_cast<const void *>(DDecl));
    }
    return DDecl->getNameAsString();
  };

  return std::visit(Overloaded{DeclaratorDeclToString,
                               [](const std::monostate) -> std::string {
                                 return "monostate";
                               }},
                    Data);
}

static const auto FunctionDeclToStringForAcquired =
    [](const FunctionDecl *const FDecl) {
      if (const auto *const Constructor = dyn_cast<CXXConstructorDecl>(FDecl)) {
        const auto *const Parent = Constructor->getParent();
        return Parent->getNameAsString();
      }
      return FDecl->getReturnType().getAsString();
    };

std::string getTransitionAcquiredTypeNames(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{FunctionDeclToStringForAcquired,
                 [](const FieldDecl *const FDecl) {
                   return FDecl->getType().getAsString();
                 },
                 [](const VarDecl *const VDecl) {
                   return VDecl->getType().getAsString();
                 },
                 [](std::monostate) -> std::string { return "monostate"; }},
      Data);
}

static const auto FunctionDeclToStringForRequired =
    [](const FunctionDecl *const FDecl) {
      const auto Parameters = FDecl->parameters();
      auto Params = fmt::format(
          "{}",
          fmt::join(Parameters |
                        ranges::views::transform([](const ParmVarDecl *PDecl) {
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
  return std::visit(
      Overloaded{
          FunctionDeclToStringForRequired,
          [](const FieldDecl *const FDecl) {
            return FDecl->getParent()->getNameAsString();
          },
          [](const VarDecl *const /*VDecl*/) -> std::string { return ""; },
          [](const std::monostate) -> std::string { return "monostate"; }},
      Data);
}

std::string toString(const TransitionType &Transition) {
  const auto &[Acquired, Function, Required] = Transition;
  return fmt::format("{} {}({})", Acquired, getTransitionName(Function),
                     fmt::join(Required, ", "));
}

std::vector<std::string> toString(const std::vector<PathType> &Paths,
                                  const GraphType &Graph,
                                  const GraphData &Data) {
  return ranges::to_vector(
      Paths | ranges::views::transform([&Graph, &Data](const PathType &Path) {
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

std::string toString(const clang::Type *const Type) {
  return clang::QualType(Type, 0).getAsString();
}
std::string toString(const clang::NamedDecl *NDecl) {
  return NDecl->getNameAsString();
}

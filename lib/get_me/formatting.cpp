#include "get_me/formatting.hpp"

#include <algorithm>
#include <variant>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclarationName.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LLVM.h>
#include <fmt/format.h>
#include <llvm/Support/Casting.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/graph.hpp"

[[nodiscard]] static clang::PrintingPolicy
normalize(clang::PrintingPolicy PrintingPolicy) {
  PrintingPolicy.SuppressTagKeyword = 1;
  return PrintingPolicy;
}

[[nodiscard]] static clang::PrintingPolicy
getNormalizedPrintingPolicy(const clang::Decl *const Decl) {
  return normalize(Decl->getASTContext().getPrintingPolicy());
}

[[nodiscard]] static std::string
getTypeAsString(const clang::ValueDecl *const VDecl) {
  return VDecl->getType().getAsString(getNormalizedPrintingPolicy(VDecl));
}

std::string getTransitionName(const TransitionDataType &Data) {
  const auto DeclaratorDeclToString =
      [](const clang::DeclaratorDecl *const DDecl) {
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

  return std::visit(DeclaratorDeclToString, Data);
}

static const auto FunctionDeclToStringForAcquired =
    [](const clang::FunctionDecl *const FDecl) {
      if (const auto *const Constructor =
              dyn_cast<clang::CXXConstructorDecl>(FDecl)) {
        const auto *const Parent = Constructor->getParent();
        return Parent->getNameAsString();
      }
      return FDecl->getReturnType().getAsString(
          getNormalizedPrintingPolicy(FDecl));
    };

std::string getTransitionAcquiredTypeNames(const TransitionDataType &Data) {
  return std::visit(Overloaded{FunctionDeclToStringForAcquired,
                               [](const clang::ValueDecl *const VDecl) {
                                 return getTypeAsString(VDecl);
                               }},
                    Data);
}

static const auto FunctionDeclToStringForRequired =
    [](const clang::FunctionDecl *const FDecl) {
      const auto Parameters = FDecl->parameters();
      auto Params = fmt::format(
          "{}",
          fmt::join(Parameters | ranges::views::transform(
                                     [](const clang::ParmVarDecl *const PDecl) {
                                       return getTypeAsString(PDecl);
                                     }),
                    ", "));
      if (const auto *const Method =
              llvm::dyn_cast<clang::CXXMethodDecl>(FDecl);
          Method != nullptr && !llvm::isa<clang::CXXConstructorDecl>(Method)) {
        const auto *const RDecl = Method->getParent();
        if (Params.empty()) {
          return RDecl->getNameAsString();
        }
        return fmt::format("{}, {}", RDecl->getNameAsString(), Params);
      }
      return Params;
    };

std::string getTransitionRequiredTypeNames(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{FunctionDeclToStringForRequired,
                 [](const clang::FieldDecl *const FDecl) {
                   return FDecl->getParent()->getNameAsString();
                 },
                 [](const clang::VarDecl *const /*VDecl*/) -> std::string {
                   return "";
                 }},
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
  if (const auto *const Decl = Type->getAsTagDecl(); Decl != nullptr) {
    return clang::QualType(Type, 0).getAsString(
        getNormalizedPrintingPolicy(Decl));
  }
  return clang::QualType(Type, 0).getAsString();
}
std::string toString(const clang::NamedDecl *const NDecl) {
  return NDecl->getNameAsString();
}

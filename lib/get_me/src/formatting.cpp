#include "get_me/formatting.hpp"

#include <string>
#include <variant>
#include <vector>

#include <boost/graph/properties.hpp>
#include <boost/property_map/property_map.hpp>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclarationName.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <llvm/Support/Casting.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/graph.hpp"
#include "get_me/path_traversal.hpp"
#include "get_me/transitions.hpp"
#include "support/ranges/ranges.hpp"
#include "support/variant.hpp"

namespace {
[[nodiscard]] clang::PrintingPolicy
normalize(clang::PrintingPolicy PrintingPolicy) {
  PrintingPolicy.SuppressTagKeyword = 1;
  return PrintingPolicy;
}

[[nodiscard]] clang::PrintingPolicy
getNormalizedPrintingPolicy(const clang::Decl *const Decl) {
  return normalize(Decl->getASTContext().getPrintingPolicy());
}

[[nodiscard]] std::string getTypeAsString(const clang::ValueDecl *const VDecl) {
  return VDecl->getType().getAsString(getNormalizedPrintingPolicy(VDecl));
}

constexpr auto FunctionDeclToStringForAcquired =
    [](const clang::FunctionDecl *const FDecl) {
      if (const auto *const Constructor =
              dyn_cast<clang::CXXConstructorDecl>(FDecl)) {
        const auto *const Parent = Constructor->getParent();
        return Parent->getNameAsString();
      }
      return FDecl->getReturnType().getAsString(
          getNormalizedPrintingPolicy(FDecl));
    };

constexpr auto FunctionDeclToStringForRequired =
    [](const clang::FunctionDecl *const FDecl) {
      auto Params = fmt::format(
          "{}", fmt::join(FDecl->parameters() |
                              ranges::views::transform(
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

constexpr auto DeclaratorDeclToString =
    [](const clang::DeclaratorDecl *const DDecl) {
      if (!DDecl->getDeclName().isIdentifier()) {
        if (const auto *const Constructor =
                llvm::dyn_cast<clang::CXXConstructorDecl>(DDecl)) {
          return Constructor->getParent()->getNameAsString();
        }

        auto NonIdentifierString =
            fmt::format("non-identifier {}({})", DDecl->getDeclKindName(),
                        static_cast<const void *>(DDecl));
        spdlog::warn(NonIdentifierString);
        return NonIdentifierString;
      }
      return DDecl->getNameAsString();
    };
} // namespace

std::string getTransitionName(const TransitionDataType &Data) {
  return std::visit(DeclaratorDeclToString, Data);
}

std::string getTransitionAcquiredTypeNames(const TransitionDataType &Data) {
  return std::visit(Overloaded{FunctionDeclToStringForAcquired,
                               [](const clang::ValueDecl *const VDecl) {
                                 return getTypeAsString(VDecl);
                               }},
                    Data);
}

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

std::vector<std::string> toString(const std::vector<PathType> &Paths,
                                  const GraphType &Graph,
                                  const GraphData &Data) {
  const auto FormatPath = [&Graph, &Data](const PathType &Path) {
    const auto GetTransition =
        [&Data, IndexMap = boost::get(boost::edge_index, Graph)](
            const EdgeDescriptor &Edge) {
          return Data.EdgeWeights[boost::get(IndexMap, Edge)];
        };

    return fmt::format(
        "{}", fmt::join(Path | ranges::views::transform(GetTransition), ", "));
  };

  return Paths | ranges::views::transform(FormatPath) | ranges::to_vector;
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

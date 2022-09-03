#include "get_me/tooling.hpp"

#include <functional>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <llvm/ADT/StringRef.h>

#include "get_me/formatting.hpp"
#include "get_me/utility.hpp"

bool GetMeVisitor::VisitFunctionDecl(clang::FunctionDecl *FDecl) {
  if (FDecl->getDeclName().isIdentifier() &&
      FDecl->getName().startswith("__")) {
    spdlog::trace("filtered due to being reserved: {}",
                  FDecl->getNameAsString());
    return true;
  }
  // FIXME: maybe need heuristic to reduce unwanted edges
  if (FDecl->getReturnType()->isArithmeticType()) {
    return true;
  }
  if (!llvm::isa<clang::CXXConstructorDecl>(FDecl) &&
      FDecl->getReturnType()->isVoidType()) {
    spdlog::trace("filtered due to returning void: {}",
                  FDecl->getNameAsString());
    return true;
  }
  if (FDecl->getReturnType().getUnqualifiedType().getAsString().find("FILE") !=
      std::string::npos) {
    return true;
  }
  if (ranges::any_of(
          FDecl->parameters(), [](const clang::ParmVarDecl *const PVDecl) {
            return PVDecl->getType().getUnqualifiedType().getAsString().find(
                       "FILE") != std::string::npos;
          })) {
    return true;
  }
  if (llvm::isa<clang::CXXDestructorDecl>(FDecl)) {
    return true;
  }
  // FIXME: allow conversions
  if (llvm::isa<clang::CXXConversionDecl>(FDecl)) {
    return true;
  }
  if (const auto *const Method = llvm::dyn_cast<clang::CXXMethodDecl>(FDecl)) {
    // FIXME: support templates
    // if (Method->getParent()->isTemplateDecl()) {
    //   return true;
    // }
    if (Method->getParent()->getName().startswith("_")) {
      return true;
    }
    if (Method->isDeleted()) {
      return true;
    }
    if (llvm::isa<clang::CXXDestructorDecl>(Method)) {
      return true;
    }
  }
  // FIXME: support templates
  // if (FDecl->isTemplateDecl()) {
  //   return true;
  // }

  if (FDecl->getReturnType().getUnqualifiedType().getAsString().starts_with(
          "_")) {
    spdlog::trace("filtered due to returning type starting with '_' (): {}",
                  FDecl->getReturnType().getUnqualifiedType().getAsString(),
                  FDecl->getNameAsString());
    return true;
  }

  if (ranges::contains(FDecl->parameters(),
                       FDecl->getReturnType().getUnqualifiedType(),
                       [](const clang::ParmVarDecl *const PVarDecl) {
                         return PVarDecl->getType().getUnqualifiedType();
                       })) {
    spdlog::trace("filtered due circular acq/req: {}",
                  FDecl->getNameAsString());
    return true;
  }

  if (ranges::contains(CollectorRef.Data,
                       TransitionDataType{FDecl->getCanonicalDecl()})) {
    return true;
  }

  CollectorRef.emplace(FDecl);
  return true;
}

bool GetMeVisitor::VisitFieldDecl(clang::FieldDecl *Field) {
  if (Field->getName().startswith("__")) {
    return true;
  }

  if (Field->getAccess() != clang::AccessSpecifier::AS_public) {
    return true;
  }
  CollectorRef.emplace(Field);
  return true;
}

void GetMe::HandleTranslationUnit(clang::ASTContext &Context) {
  // Traversing the translation unit decl via a RecursiveASTVisitor
  // will visit all nodes in the AST.
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());

static void filterOverloads(std::vector<TransitionDataType> &Data,
                            size_t OverloadFilterParameterCountThreshold = 0) {
  const auto GetName = [](const TransitionDataType &Val) {
    const auto GetNameOfDeclaratorDecl =
        [](const clang::DeclaratorDecl *const DDecl) -> std::string {
      if (!DDecl->getDeclName().isIdentifier()) {
        if (const auto *const Constructor =
                llvm::dyn_cast<clang::CXXConstructorDecl>(DDecl)) {
          return fmt::format("constructor of {}",
                             Constructor->getParent()->getNameAsString());
        }
        return "non-identifier";
      }
      return DDecl->getName().str();
    };
    return std::visit(
        Overloaded{GetNameOfDeclaratorDecl,
                   [](std::monostate) -> std::string { return "monostate"; }},
        Val);
  };
    const auto GetParameters = [](const TransitionDataType &Val) {
      return std::visit(
          Overloaded{
              [](const clang::FunctionDecl *const CurrentDecl)
                  -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
                return CurrentDecl->parameters();
              },
              [](auto) -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
                return {};
              }},
          Val);
    };
  const auto Comparator =
      [&GetName, &GetParameters, OverloadFilterParameterCountThreshold](
          const TransitionDataType &Lhs, const TransitionDataType &Rhs) {
        if (const auto IndexComparison = Lhs.index() <=> Rhs.index();
            std::is_neq(IndexComparison)) {
          return std::is_lt(IndexComparison);
        }
        if (const auto NameComparison = GetName(Lhs) <=> GetName(Rhs);
            std::is_neq(NameComparison)) {
          return std::is_lt(NameComparison);
        }
    const auto LhsParams = GetParameters(Lhs);
    if (!LhsParams) {
      return true;
    }
    const auto RhsParams = GetParameters(Rhs);
    if (!RhsParams) {
      return false;
    }

        if (LhsParams->empty()) {
          return true;
        }
        if (RhsParams->empty()) {
          return false;
        }

        const auto Projection = [](const clang::ParmVarDecl *const PVarDecl) {
          return PVarDecl->getType();
        };
    const auto MismatchResult =
            ranges::mismatch(LhsParams.value(), RhsParams.value(),
                             std::equal_to{}, Projection, Projection);
        const auto Res = MismatchResult.in1 == LhsParams.value().end();
        if (Res) {
          return std::distance(LhsParams->begin(), MismatchResult.in1) <
                 OverloadFilterParameterCountThreshold;
        }
        return Res;
  };
  // sort data, this sorts overloads by their number of parameters
  // FIXME: sorting just to make sure the overloads with longer parameter lists
  // are removed, figure out a better way. The algo also depends on this order
  // to determine if it is an overload
  ranges::sort(Data, Comparator);
  const auto IsOverload = [&GetName,
                           &GetParameters](const TransitionDataType &Lhs,
                                           const TransitionDataType &Rhs) {
        if (Lhs.index() != Rhs.index()) {
          return false;
        }
    if (GetName(Lhs) != GetName(Rhs)) {
      return false;
    }
    const auto LhsParams = GetParameters(Lhs);
    if (!LhsParams) {
      return false;
    }
    const auto RhsParams = GetParameters(Rhs);
    if (!RhsParams) {
      return false;
    }

    if (LhsParams->empty()) {
      return false;
    }
    if (RhsParams->empty()) {
      return false;
    }

    const auto Projection = [](const clang::ParmVarDecl *const PVarDecl) {
      return PVarDecl->getType()->getUnqualifiedDesugaredType();
    };
    const auto MismatchResult =
        ranges::mismatch(LhsParams.value(), RhsParams.value(), std::equal_to{},
                         Projection, Projection);
    return MismatchResult.in1 == LhsParams.value().end();
  };
  const auto UniqueEndIter = ranges::unique(Data, IsOverload);

  auto Res = fmt::format("erasing: [");
  for (auto Iter = UniqueEndIter; Iter != Data.end(); ++Iter) {
    Res = fmt::format("{}{}, ", Res, *Iter);
  }
  Res = fmt::format("{}] from {}", Res, Data);
  Data.erase(UniqueEndIter, Data.end());
  spdlog::trace("{}, post erasure: {}", Res, Data);
}

void GetMe::HandleTranslationUnit(clang::ASTContext &Context) {
  // Traversing the translation unit decl via a RecursiveASTVisitor
  // will visit all nodes in the AST.
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
}

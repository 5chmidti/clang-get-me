#include "get_me/tooling.hpp"

#include <llvm/ADT/StringRef.h>

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
  if (ranges::any_of(FDecl->parameters(),
                     [](const clang::ParmVarDecl *const PVDecl) {
                       return PVDecl->getType().getAsString().find("FILE") !=
                              std::string::npos;
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
  const auto Comparator = [&GetName](const TransitionDataType &Lhs,
                                     const TransitionDataType &Rhs) {
    if (Lhs.index() != Rhs.index()) {
      return Lhs.index() < Rhs.index();
    }
    const auto LhsName = GetName(Lhs);
    const auto RhsName = GetName(Rhs);
    if (LhsName != RhsName) {
      return LhsName < RhsName;
    }
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
    const auto LhsParams = GetParameters(Lhs);
    if (!LhsParams) {
      return true;
    }
    const auto RhsParams = GetParameters(Rhs);
    if (!RhsParams) {
      return false;
    }

    const auto MismatchResult =
        ranges::mismatch(LhsParams.value(), RhsParams.value());
    return MismatchResult.in1 == LhsParams.value().end();
  };
  // sort data, this sorts overloads by their number of parameters
  ranges::sort(Visitor.CollectorRef.Data, Comparator);
  const auto UniqueEndIter = ranges::unique(
      Visitor.CollectorRef.Data,
      [&GetName](const TransitionDataType &Lhs, const TransitionDataType &Rhs) {
        if (Lhs.index() != Rhs.index()) {
          return false;
        }
        const auto LhsName = GetName(Lhs);
        const auto RhsName = GetName(Rhs);
        const auto Res = LhsName == RhsName;
        return Res;
      });

  auto Res = fmt::format("erasing: [");
  for (auto Iter = UniqueEndIter; Iter != Visitor.CollectorRef.Data.end();
       ++Iter) {
    Res = fmt::format("{}{}, ", Res, *Iter);
  }
  spdlog::trace("{}] from {}", Res, Visitor.CollectorRef.Data);
  Visitor.CollectorRef.Data.erase(UniqueEndIter,
                                  Visitor.CollectorRef.Data.end());
}
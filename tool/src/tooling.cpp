#include "get_me/tooling.hpp"

#include <functional>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <llvm/ADT/StringRef.h>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"
#include "get_me/utility.hpp"

[[nodiscard]] static bool
ignoreFILEPredicate(const clang::FunctionDecl *FDecl) {
  if (FDecl->getReturnType().getUnqualifiedType().getAsString().find("FILE") !=
      std::string::npos) {
    return true;
  }
  return ranges::any_of(
      FDecl->parameters(), [](const clang::ParmVarDecl *const PVDecl) {
        return PVDecl->getType().getUnqualifiedType().getAsString().find(
                   "FILE") != std::string::npos;
      });
}

[[nodiscard]] static bool
reservedIntentifiersPredicate(const clang::FunctionDecl *FDecl) {
  if (FDecl->getDeclName().isIdentifier() &&
      FDecl->getName().startswith("__")) {
    return true;
  }
  return FDecl->getReturnType().getUnqualifiedType().getAsString().starts_with(
      "_");
}

[[nodiscard]] static bool
requiredContainsAcquiredPredicate(const clang::FunctionDecl *FDecl) {
  return ranges::contains(FDecl->parameters(),
                          FDecl->getReturnType().getUnqualifiedType(),
                          [](const clang::ParmVarDecl *const PVarDecl) {
                            return PVarDecl->getType().getUnqualifiedType();
                          });
}

[[nodiscard]] static bool
functionFilterPredicate(const clang::FunctionDecl *FDecl,
                        TransitionCollector &Collector) {
  if (reservedIntentifiersPredicate(FDecl)) {
    return true;
  }
  if (ignoreFILEPredicate(FDecl)) {
    return true;
  }
  // FIXME: maybe need heuristic to reduce unwanted edges
  if (FDecl->getReturnType()->isArithmeticType()) {
    spdlog::trace("filtered due to returning arithmetic type: {}",
                  FDecl->getNameAsString());
    return true;
  }
  if (requiredContainsAcquiredPredicate(FDecl)) {
    spdlog::trace("filtered due to require-acquire cycle: {}",
                  FDecl->getNameAsString());
    return true;
  }
  // FIXME: support templates
  // if (FDecl->isTemplateDecl()) {
  //   return true;
  // }

  if (ranges::contains(Collector, TransitionDataType{FDecl->getCanonicalDecl()},
                       [](const auto &Val) { return std::get<1>(Val); })) {
    return true;
  }

  return false;
}

bool GetMeVisitor::VisitFunctionDecl(clang::FunctionDecl *FDecl) {
  if (llvm::isa<clang::CXXMethodDecl>(FDecl)) {
    return true;
  }
  if (functionFilterPredicate(FDecl, CollectorRef)) {
    return true;
  }

  auto [Acquired, Required] = toTypeSet(FDecl);
  CollectorRef.emplace_back(std::move(Acquired), TransitionDataType{FDecl},
                            std::move(Required));
  return true;
}

bool GetMeVisitor::VisitFieldDecl(clang::FieldDecl *Field) {
  if (Field->getName().startswith("__")) {
    return true;
  }

  if (Field->getAccess() != clang::AccessSpecifier::AS_public) {
    return true;
  }

  auto [Acquired, Required] = toTypeSet(Field);
  CollectorRef.emplace_back(std::move(Acquired), TransitionDataType{Field},
                            std::move(Required));
  return true;
}

// FIXME: implement inheritance
// FIXME: also do this for FieldDecls
bool GetMeVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl *RDecl) {
  if (RDecl->getName().startswith("_")) {
    return true;
  }
  for (const clang::CXXMethodDecl *Method : RDecl->methods()) {
    // FIXME: allow conversions
    if (llvm::isa<clang::CXXConversionDecl>(Method)) {
      return true;
    }
    if (llvm::isa<clang::CXXDestructorDecl>(Method)) {
      continue;
    }
    if (Method->isDeleted()) {
      continue;
    }
    if (functionFilterPredicate(Method, CollectorRef)) {
      continue;
    }

    auto [Acquired, Required] = toTypeSet(Method);
    CollectorRef.emplace_back(std::move(Acquired), TransitionDataType{Method},
                              std::move(Required));
  }

  // add non user provided default constructor
  if (RDecl->hasDefaultConstructor() &&
      !RDecl->hasUserProvidedDefaultConstructor()) {
    CollectorRef.emplace_back(
        TypeSet{TypeSetValueType{RDecl->getTypeForDecl()}},
        TransitionDataType{RDecl->getNameAsString()}, TypeSet{});
  }
  return true;
}

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
        Overloaded{
            GetNameOfDeclaratorDecl,
            [](const CustomTransitionType &CustomVal) { return CustomVal; },
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
          return static_cast<size_t>(
                     std::distance(LhsParams->begin(), MismatchResult.in1)) <
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
  spdlog::trace("collected: {}", Visitor.CollectorRef);
}

bool GetMeVisitor::VisitVarDecl(clang::VarDecl *VDecl) {
  if (VDecl->isCXXInstanceMember()) {
    return true;
  }
  if (!VDecl->isStaticDataMember()) {
    return true;
  }
  if (const auto *const RDecl =
          llvm::dyn_cast<clang::RecordDecl>(VDecl->getDeclContext())) {
    CollectorRef.emplace_back(
        TypeSet{
            TypeSetValueType{VDecl->getType().getCanonicalType().getTypePtr()}},
        TransitionDataType{VDecl}, TypeSet{});
  }
  return true;
}

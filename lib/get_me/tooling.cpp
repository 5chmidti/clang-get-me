#include "get_me/tooling.hpp"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <boost/container/flat_set.hpp>
#include <boost/container/vector.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/named_function_params.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/pending/property.hpp>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclObjC.h>
#include <clang/AST/DeclOpenMP.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Redeclarable.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/StmtIterator.h>
#include <clang/AST/Type.h>
#include <clang/Sema/Sema.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/Support/Casting.h>
#include <range/v3/action/action.hpp>
#include <range/v3/action/insert.hpp>
#include <range/v3/action/remove.hpp>
#include <range/v3/action/reverse.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/action/unique.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/mismatch.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/traits.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/cache1.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/direct_type_dependency_propagation.hpp"
#include "get_me/formatting.hpp"
#include "get_me/graph.hpp"
#include "get_me/indexed_graph_sets.hpp"
#include "get_me/tooling_filters.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "get_me/utility.hpp"

template <typename T>
[[nodiscard]] static TransitionType toTransitionType(const T *const Transition,
                                                     const Config &Conf) {

  auto [Acquired, Required] = toTypeSet(Transition, Conf);
  return {std::move(Acquired), TransitionDataType{Transition},
          std::move(Required)};
}

class GetMeVisitor : public clang::RecursiveASTVisitor<GetMeVisitor> {
public:
  GetMeVisitor(const Config &Configuration, TransitionCollector &TransitionsRef,
               std::vector<const clang::CXXRecordDecl *> &CXXRecordsRef,
               std::vector<const clang::TypedefNameDecl *> &TypedefNameDeclsRef,
               clang::Sema &SemaRef)
      : Conf_{Configuration},
        Transitions_{TransitionsRef},
        CxxRecords_{CXXRecordsRef},
        TypedefNameDecls_{TypedefNameDeclsRef},
        Sema_{SemaRef} {}

  [[nodiscard]] bool TraverseDecl(clang::Decl *Decl) {
    if (Decl == nullptr) {
      return true;
    }
    if (!Conf_.EnableFilterStd || !Decl->isInStdNamespace()) {
      clang::RecursiveASTVisitor<GetMeVisitor>::TraverseDecl(Decl);
    }
    return true;
  }

  [[nodiscard]] bool VisitFunctionDecl(clang::FunctionDecl *FDecl) {
    // handled differently via iterating over a CXXRecord's methods
    if (llvm::isa<clang::CXXMethodDecl>(FDecl)) {
      return true;
    }
    if (filterOut(FDecl, Conf_)) {
      return true;
    }

    Transitions_.emplace(toTransitionType(FDecl, Conf_));
    return true;
  }

  [[nodiscard]] bool VisitFieldDecl(clang::FieldDecl *FDecl) {
    if (hasReservedIdentifierNameOrType(FDecl)) {
      return true;
    }

    // FIXME: filter access spec for members, depends on context of query

    if (FDecl->getType()->isArithmeticType()) {
      return true;
    }
    if (hasTypeNameContainingName(FDecl, "exception")) {
      return true;
    }

    Transitions_.emplace(toTransitionType(FDecl, Conf_));
    return true;
  }

  [[nodiscard]] bool VisitCXXRecordDecl(clang::CXXRecordDecl *RDecl) {
    if (filterOut(RDecl, Conf_)) {
      return true;
    }
    const auto AddToTransitions =
        [this](const ranges::viewable_range auto Range) {
          ranges::transform(
              Range | ranges::views::filter([this](const auto *const Function) {
                return !filterOut(Function, Conf_);
              }),
              std::inserter(Transitions_, Transitions_.end()),
              [this](const auto *const Function) {
                return toTransitionType(Function, Conf_);
              });
        };
    AddToTransitions(RDecl->methods());

    // declare implicit default constructor even if it is not used
    if (RDecl->needsImplicitDefaultConstructor()) {
      Sema_.DeclareImplicitDefaultConstructor(RDecl);
    }

    AddToTransitions(RDecl->ctors());

    if (RDecl->getNumBases() != 0) {
      CxxRecords_.push_back(RDecl);
    }
    return true;
  }

  [[nodiscard]] bool
  VisitTemplateSpecializationType(clang::TemplateSpecializationType *TSType) {
    if (auto *const RDecl = TSType->getAsCXXRecordDecl()) {
      std::ignore = VisitCXXRecordDecl(RDecl);
    }
    return true;
  }

  [[nodiscard]] bool VisitVarDecl(clang::VarDecl *VDecl) {
    if (!VDecl->isStaticDataMember()) {
      return true;
    }
    if (Conf_.EnableFilterStd && VDecl->isInStdNamespace()) {
      return true;
    }
    if (VDecl->getType()->isArithmeticType()) {
      return true;
    }
    if (hasReservedIdentifierNameOrType(VDecl)) {
      return true;
    }
    if (hasTypeNameContainingName(VDecl, "exception")) {
      return true;
    }

    Transitions_.emplace(std::get<0>(toTypeSet(VDecl, Conf_)),
                         TransitionDataType{VDecl}, TypeSet{});
    return true;
  }

  [[nodiscard]] bool VisitTypedefNameDecl(clang::TypedefNameDecl *NDecl) {
    if (NDecl->isInvalidDecl()) {
      return true;
    }
    if (Conf_.EnableFilterStd && NDecl->isInStdNamespace()) {
      return true;
    }
    if (NDecl->getUnderlyingType()->isArithmeticType()) {
      return true;
    }
    if (NDecl->isTemplateDecl()) {
      return true;
    }
    // FIXME: use this to explicitly create the type
    assert(NDecl->getASTContext()
               .getTypedefType(NDecl, NDecl->getUnderlyingType())
               .getTypePtr());
    if (NDecl->getTypeForDecl() == nullptr ||
        NDecl->getUnderlyingType().getTypePtr() == nullptr) {
      return true;
    }
    if (hasReservedIdentifierNameOrType(NDecl)) {
      return true;
    }
    TypedefNameDecls_.push_back(NDecl);
    return true;
  }

private:
  const Config &Conf_;
  TransitionCollector &Transitions_;
  std::vector<const clang::CXXRecordDecl *> &CxxRecords_;
  std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls_;
  clang::Sema &Sema_;
};

[[nodiscard]] static auto getParametersOpt(const TransitionDataType &Val) {
  return std::visit(
      Overloaded{
          [](const clang::FunctionDecl *const CurrentDecl)
              -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
            return CurrentDecl->parameters();
          },
          [](auto &&) -> std::optional<clang::ArrayRef<clang::ParmVarDecl *>> {
            return {};
          }},
      Val);
};

static void filterOverloads(TransitionCollector &Transitions,
                            size_t OverloadFilterParameterCountThreshold = 0) {

  const auto Comparator =
      [OverloadFilterParameterCountThreshold](const TransitionDataType &Lhs,
                                              const TransitionDataType &Rhs) {
        if (const auto IndexComparison = Lhs.index() <=> Rhs.index();
            std::is_neq(IndexComparison)) {
          return std::is_lt(IndexComparison);
        }
        if (const auto NameComparison =
                getTransitionName(Lhs) <=> getTransitionName(Rhs);
            std::is_neq(NameComparison)) {
          return std::is_lt(NameComparison);
        }
        const auto LhsParams = getParametersOpt(Lhs);
        if (!LhsParams) {
          return true;
        }
        const auto RhsParams = getParametersOpt(Rhs);
        if (!RhsParams) {
          return false;
        }

        if (LhsParams->empty()) {
          return true;
        }
        if (RhsParams->empty()) {
          return false;
        }

        if (const auto MismatchResult =
                ranges::mismatch(LhsParams.value(), RhsParams.value(),
                                 std::equal_to{}, ToQualType, ToQualType);
            MismatchResult.in1 == LhsParams.value().end()) {
          return static_cast<size_t>(
                     std::distance(LhsParams->begin(), MismatchResult.in1)) <
                 OverloadFilterParameterCountThreshold;
        }
        return false;
      };
  const auto IsOverload = [](const TransitionType &LhsTuple,
                             const TransitionType &RhsTuple) {
    const auto &Lhs = transition(LhsTuple);
    const auto &Rhs = transition(RhsTuple);
    if (Lhs.index() != Rhs.index()) {
      return false;
    }
    if (getTransitionName(Lhs) != getTransitionName(Rhs)) {
      return false;
    }
    const auto LhsParams = getParametersOpt(Lhs);
    if (!LhsParams) {
      return false;
    }
    const auto RhsParams = getParametersOpt(Rhs);
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

  // FIXME: sorting just to make sure the overloads with longer parameter lists
  // are removed, figure out a better way. The algo also depends on this order
  // to determine if it is an overload
  Transitions = std::move(Transitions) | ranges::to_vector |
                ranges::actions::sort(Comparator, transition) |
                ranges::actions::unique(IsOverload) |
                ranges::to<TransitionCollector>;
}

void GetMe::HandleTranslationUnit(clang::ASTContext &Context) {
  std::vector<const clang::CXXRecordDecl *> CXXRecords{};
  std::vector<const clang::TypedefNameDecl *> TypedefNameDecls{};
  GetMeVisitor Visitor{Conf_, Transitions_, CXXRecords, TypedefNameDecls,
                       Sema_};

  std::ignore = Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  if (Conf_.EnableFilterOverloads) {
    filterOverloads(Transitions_);
  }

  const auto PreSize = Transitions_.size();
  // FIXME: use config
  propagateTransitionsOfDirectTypeDependencies(Transitions_, CXXRecords,
                                               TypedefNameDecls);
  const auto PostSize = Transitions_.size();
  // spdlog::info("Transitions: {}", Transitions_);
  // spdlog::info("propagateTransitionsOfDirectTypeDependencies: {} -> {}",
  //              PreSize, PostSize);
}

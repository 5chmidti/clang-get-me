#include "get_me/propagate_inheritance.hpp"

#include <boost/graph/breadth_first_search.hpp>
#include <clang/AST/DeclCXX.h>
#include <range/v3/action/reverse.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/cache1.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/unique.hpp>

#include "get_me/direct_type_dependency_propagation.hpp"
#include "get_me/graph.hpp"
#include "support/ranges/functional.hpp"
#include "support/ranges/ranges.hpp"
#include "support/variant.hpp"

namespace {
[[nodiscard]] auto getVerticesWithNoInEdges(const DTDGraphData &Data) {
  return ranges::views::indices(Data.VertexData.size()) |
         ranges::views::set_difference(Data.Edges |
                                       ranges::views::transform(Element<1>) |
                                       ranges::views::unique);
}

[[nodiscard]] bool overridesMethod(const TypeSetValueType &TypeValue,
                                   const clang::CXXMethodDecl *const Method) {
  return std::visit(
      Overloaded{[Method](const clang::Type *const Type) {
                   const auto *const NewTypeAsRecordType =
                       llvm::dyn_cast<clang::RecordType>(Type);
                   if (NewTypeAsRecordType == nullptr) {
                     return false;
                   }
                   const auto *const Derived =
                       NewTypeAsRecordType->getAsCXXRecordDecl();

                   if (Derived == nullptr) {
                     return false;
                   }

                   const auto OverridesMethod =
                       [Method](
                           const clang::CXXMethodDecl *const DerivedMethod) {
                         return DerivedMethod->getNameAsString() ==
                                    Method->getNameAsString() &&
                                DerivedMethod->getType() == Method->getType();
                       };
                   return !ranges::any_of(Derived->methods(), OverridesMethod);
                 },
                 [](auto &&) { return false; }},
      TypeValue);
}

[[nodiscard]] bool isOverriddenBy(const clang::CXXMethodDecl *const Ctor,
                                  const clang::CXXRecordDecl *const Derived) {
  const auto OverridesCtor =
      [Ctor](const clang::CXXConstructorDecl *const DerivedCtor) {
        return DerivedCtor->getNumParams() == Ctor->getNumParams() &&
               ranges::equal(Ctor->parameters(), DerivedCtor->parameters(),
                             std::less{}, ToQualType, ToQualType);
      };
  return !ranges::any_of(Derived->ctors(), OverridesCtor);
}

[[nodiscard]] bool
overridesConstructor(const TypeSetValueType &TypeValue,
                     const clang::CXXMethodDecl *const Ctor) {
  return std::visit(
      Overloaded{[Ctor](const clang::Type *const Type) {
                   const auto *const NewTypeAsRecordType =
                       llvm::dyn_cast<clang::RecordType>(Type);
                   if (NewTypeAsRecordType == nullptr) {
                     return false;
                   }
                   const auto *const Derived =
                       NewTypeAsRecordType->getAsCXXRecordDecl();

                   if (Derived == nullptr) {
                     return false;
                   }

                   if (!Derived->isDerivedFrom(Ctor->getParent())) {
                     return false;
                   }

                   return isOverriddenBy(Ctor, Derived);
                 },
                 [](auto &&) { return false; }},
      TypeValue);
}

[[nodiscard]] auto transitionIsMember(const TypeSetValueType &DerivedType) {
  return [DerivedType](const StrippedTransitionType &Transition) {
    return std::visit(
        Overloaded{[](const clang::FieldDecl *const) { return false; },
                   [DerivedType](const clang::FunctionDecl *const FDecl) {
                     const auto *const Method =
                         llvm::dyn_cast<clang::CXXMethodDecl>(FDecl);
                     if (Method == nullptr) {
                       return false;
                     }
                     if (const auto *const Ctor =
                             llvm::dyn_cast<clang::CXXConstructorDecl>(Method);
                         Ctor != nullptr) {
                       return overridesConstructor(DerivedType, Ctor);
                     }

                     return overridesMethod(DerivedType, Method);
                   },
                   [](const auto *const) { return false; }},
        ToTransition(Transition));
  };
}

class InheritanceGraphBuilder {
public:
  void visit(const clang::CXXRecordDecl *const Record) {
    const auto RecordIndex = addType(launderType(Record->getTypeForDecl()));
    visitCXXRecordDeclImpl(Record, RecordIndex);
  }

  [[nodiscard]] DTDGraphData getResult() {
    return {getIndexedSetSortedByIndex(std::move(Vertices_)),
            getIndexedSetSortedByIndex(std::move(Edges_))};
  }

private:
  [[nodiscard]] VertexDescriptor addType(const TypeSetValueType &Type) {
    const auto BaseTypeIter = Vertices_.find(Type);
    const auto BaseTypeExists = BaseTypeIter != Vertices_.end();
    const auto BaseVertexIndex =
        BaseTypeExists ? Index(*BaseTypeIter) : Vertices_.size();
    if (!BaseTypeExists) {
      Vertices_.emplace(BaseVertexIndex, Type);
    }
    return BaseVertexIndex;
  }

  void visitCXXRecordDeclImpl(const clang::CXXRecordDecl *const Derived,
                              const VertexDescriptor DerivedIndex) {
    ranges::for_each(
        Derived->bases(),
        [this, DerivedIndex](const clang::CXXBaseSpecifier &BaseSpec) {
          const auto QType = BaseSpec.getType();
          const auto BaseVertexIndex = addType(launderType(QType.getTypePtr()));

          if (addEdge(DTDGraphData::EdgeType{BaseVertexIndex, DerivedIndex})) {
            visitCXXRecordDeclImpl(QType->getAsCXXRecordDecl(),
                                   BaseVertexIndex);
          }
        });
  }

  [[nodiscard]] bool addEdge(const DTDGraphData::EdgeType &EdgeToAdd) {
    const auto EdgesIter = Edges_.lower_bound(EdgeToAdd);
    if (const auto ContainsEdgeToAdd =
            EdgesIter != ranges::end(Edges_) && Value(*EdgesIter) == EdgeToAdd;
        !ContainsEdgeToAdd) {
      Edges_.emplace_hint(EdgesIter, Edges_.size(), EdgeToAdd);
      return true;
    }
    return false;
  }

  indexed_set<TypeSetValueType> Vertices_{};
  indexed_set<DTDGraphData::EdgeType> Edges_{};
};

[[nodiscard]] DTDGraphData createInheritanceGraph(
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords) {
  auto Builder = InheritanceGraphBuilder{};

  const auto Visitor = [&Builder](const auto *const Value) {
    Builder.visit(Value);
  };

  ranges::for_each(CXXRecords, Visitor);

  return Builder.getResult();
}

[[nodiscard]] auto maybePropagate(const TypeSetValueType &SourceType,
                                  const TypeSetValueType TargetType) {
  return
      [SourceType, TargetType](const StrippedTransitionType &StrippedTransition)
          -> std::pair<bool, StrippedTransitionType> {
        auto RequiredTS = ToRequired(StrippedTransition);
        const auto ChangedRequiredTS = 0U != RequiredTS.erase(SourceType);
        RequiredTS.insert(TargetType);
        return {ChangedRequiredTS,
                StrippedTransitionType{ToTransition(StrippedTransition),
                                       RequiredTS}};
      };
}

[[nodiscard]] std::vector<VertexDescriptor>
getVerticesToVisit(ranges::range auto Sources, const DTDGraphType &Graph) {
  class BFSEdgeCollector : public boost::default_bfs_visitor {
  public:
    explicit BFSEdgeCollector(std::vector<VertexDescriptor> &Collector)
        : Collector_{Collector} {}

    void examine_vertex(VertexDescriptor Vertex,
                        const DTDGraphType & /*Graph*/) {
      Collector_.emplace_back(Vertex);
    }

  private:
    std::vector<VertexDescriptor> &Collector_;
  };

  const auto CollectEdges = [&Graph](const VertexDescriptor Vertex) {
    std::vector<VertexDescriptor> Vertices{};
    boost::breadth_first_search(Graph, Vertex,
                                boost::visitor(BFSEdgeCollector{Vertices}));
    return Vertices;
  };

  return Sources | ranges::views::for_each(CollectEdges) | ranges::to_vector |
         ranges::actions::reverse;
}

class InheritancePropagator {
private:
  [[nodiscard]] const TypeSetValueType &
  toType(const VertexDescriptor Vertex) const {
    return Data_.VertexData[Vertex];
  }

  [[nodiscard]] auto propagateRequired() const {
    return [this](const EdgeDescriptor &Edge) {
      return Transitions_ |
             ranges::views::transform(
                 [MaybePropagate = maybePropagate(toType(Source(Edge)),
                                                  toType(Target(Edge)))](
                     const BundeledTransitionType &BundeledTransition)
                     -> BundeledTransitionType {
                   return {ToAcquired(BundeledTransition),
                           BundeledTransition.second |
                               ranges::views::transform(MaybePropagate) |
                               ranges::views::cache1 |
                               ranges::views::filter(Element<0>) |
                               ranges::views::transform(Element<1>) |
                               ranges::to<StrippedTransitionsSet>};
                 });
    };
  }

  [[nodiscard]] auto
  propagatedForAcquired(const TypeSetValueType &DerivedType,
                        const TypeSetValueType &BaseType) const {
    return Transitions_ |
           ranges::views::filter(EqualTo(DerivedType), ToAcquired) |
           ranges::views::transform(
               [BaseType](const BundeledTransitionType &BundeledTransition)
                   -> BundeledTransitionType {
                 return {BaseType, BundeledTransition.second};
               });
  }

  [[nodiscard]] auto propagatedInheritedMethodsForAcquired(
      const TypeSetValueType &BaseType,
      const TypeSetValueType &DerivedType) const {
    return Transitions_ | ranges::views::filter(EqualTo(BaseType), ToAcquired) |
           ranges::views::transform(
               [DerivedType](const BundeledTransitionType &BundeledTransition)
                   -> BundeledTransitionType {
                 return {DerivedType, BundeledTransition.second |
                                          ranges::views::filter(
                                              transitionIsMember(DerivedType)) |
                                          ranges::to<StrippedTransitionsSet>};
               });
  };

  [[nodiscard]] auto propagateAcquiredInheritance() const {
    return [this](const EdgeDescriptor &Edge) {
      const auto &BaseType = toType(Source(Edge));
      const auto &DerivedType = toType(Target(Edge));

      return ranges::views::concat(
          propagatedInheritedMethodsForAcquired(BaseType, DerivedType),
          propagatedForAcquired(DerivedType, BaseType));
    };
  }

public:
  InheritancePropagator(TransitionCollector &TransitionsRef, DTDGraphData Data)
      : Transitions_{TransitionsRef},
        Data_{std::move(Data)},
        Graph_{createGraph(Data_)} {}

  void operator()() {
    const auto Sources = getVerticesWithNoInEdges(Data_);
    const auto VerticesToVisit = getVerticesToVisit(Sources, Graph_);

    const auto PropagateTransitionsOfOutEdges =
        [this](const VertexDescriptor Vertex) {
          VertexVisited_[Vertex] = true;
          return toRange(boost::out_edges(Vertex, Graph_)) |
                 propagate(propagateRequired(), propagateAcquiredInheritance());
        };

    std::vector<BundeledTransitionType> Vec =
        VerticesToVisit |
        ranges::views::filter(ranges::not_fn(Lookup(VertexVisited_))) |
        ranges::views::for_each(PropagateTransitionsOfOutEdges) |
        ranges::to_vector;
    ranges::for_each(Vec, [this](BundeledTransitionType NewTransitions) {
      Transitions_[ToAcquired(NewTransitions)].merge(
          std::move(NewTransitions.second));
    });
  }

private:
  TransitionCollector &Transitions_;
  DTDGraphData Data_;
  DTDGraphType Graph_;
  std::vector<bool> VertexVisited_ = std::vector<bool>(Data_.VertexData.size());
};
} // namespace

void propagateInheritance(
    TransitionCollector &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords) {
  InheritancePropagator{Transitions, createInheritanceGraph(CXXRecords)}();
}

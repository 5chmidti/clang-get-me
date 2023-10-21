#include "get_me/propagate_inheritance.hpp"

#include <functional>
#include <utility>
#include <variant>
#include <vector>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/detail/adjacency_list.hpp>
#include <boost/graph/graph_selectors.hpp>
#include <boost/graph/graph_traits.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <llvm/Support/Casting.h>
#include <range/v3/action/reverse.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/functional/not_fn.hpp>
#include <range/v3/range/access.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/unique.hpp>

#include "get_me/direct_type_dependency_propagation.hpp"
#include "get_me/graph.hpp"
#include "get_me/indexed_set.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/functional.hpp"
#include "support/ranges/functional_clang.hpp"
#include "support/ranges/ranges.hpp"
#include "support/variant.hpp"

namespace {
using InheritanceGraph =
    boost::adjacency_list<boost::listS, boost::vecS, boost::directedS>;

using EdgeDescriptor =
    typename boost::graph_traits<InheritanceGraph>::edge_descriptor;
using VertexDescriptor =
    typename boost::graph_traits<InheritanceGraph>::vertex_descriptor;

[[nodiscard]] auto getVerticesWithNoInEdges(const DTDGraphData &Data) {
  return ranges::views::indices(Data.VertexData.size()) |
         ranges::views::set_difference(Data.Edges |
                                       ranges::views::transform(Target) |
                                       ranges::views::unique);
}

[[nodiscard]] bool overridesMethod(const TypeSetValueType &TypeValue,
                                   const clang::CXXMethodDecl *const Method) {
  return std::visit(
      Overloaded{[Method](const clang::QualType &QType) {
                   const auto *const Derived = QType->getAsCXXRecordDecl();

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
      Overloaded{[Ctor](const clang::QualType &QType) {
                   const auto *const Derived = QType->getAsCXXRecordDecl();

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
  return [DerivedType](const TransitionDataType &Transition) {
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
        Transition);
  };
}

[[nodiscard]] InheritanceGraph createGraph(const DTDGraphData &Data) {
  return {Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
          Data.VertexData.size()};
}

class InheritanceGraphBuilder {
public:
  void visit(const clang::CXXRecordDecl *const Record) {
    const auto RecordIndex =
        addType(clang::QualType{Record->getTypeForDecl(), 0});
    visitCXXRecordDecl(Record, RecordIndex);
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

  void visitCXXRecordDecl(const clang::CXXRecordDecl *const Derived,
                          const VertexDescriptor DerivedIndex) {
    if (Derived == nullptr) {
      return;
    }
    ranges::for_each(
        Derived->bases(),
        [this, DerivedIndex](const clang::CXXBaseSpecifier &BaseSpec) {
          const auto QType = BaseSpec.getType();
          const auto BaseVertexIndex = addType(QType);

          if (addEdge(DTDGraphData::EdgeType{BaseVertexIndex, DerivedIndex})) {
            visitCXXRecordDecl(QType->getAsCXXRecordDecl(), BaseVertexIndex);
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
    GetMeException::verify(Value != nullptr, "starts with nullptr");
    Builder.visit(Value);
  };

  ranges::for_each(CXXRecords, Visitor);

  return Builder.getResult();
}

[[nodiscard]] std::vector<VertexDescriptor>
getVerticesToVisit(ranges::range auto Sources, const InheritanceGraph &Graph) {
  class BFSEdgeCollector : public boost::default_bfs_visitor {
  public:
    explicit BFSEdgeCollector(std::vector<VertexDescriptor> &Collector)
        : Collector_{Collector} {}

    void examine_vertex(VertexDescriptor Vertex,
                        const InheritanceGraph & /*Graph*/) {
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

template <typename... Ts> [[nodiscard]] auto propagate(Ts &&...Propagators) {
  return ranges::views::for_each(
      [... Propagators =
           std::forward<Ts>(Propagators)]<typename T>(const T &Value) {
        return ranges::views::concat(Propagators(Value)...);
      });
}

[[nodiscard]] std::pair<bool, std::pair<TypeSetValueType, TypeSet>>
swapRequiredType(TransitionType::first_type Transition,
                 const TypeSetValueType &SourceType,
                 const TypeSetValueType TargetType) {
  auto Required = ToRequired(Transition);
  const auto ChangedRequiredTS = 0U != Required.erase(SourceType);
  if (ChangedRequiredTS) {
    Required.insert(TargetType);
  }
  return std::pair{ChangedRequiredTS,
                   std::pair{ToAcquired(Transition), Required}};
}

class InheritancePropagator {
private:
  [[nodiscard]] const TypeSetValueType &
  toType(const VertexDescriptor Vertex) const {
    return Data_.VertexData[Vertex];
  }

  [[nodiscard]] auto propagateRequired() {
    return [this](const EdgeDescriptor &Edge) {
      return Transitions_.Data |
             ranges::views::transform(
                 [this, Edge](const TransitionType &Transition) {
                   return std::pair{swapRequiredType(Transition.first,
                                                     toType(Source(Edge)),
                                                     toType(Target(Edge))),
                                    Transition.second};
                 }) |
             ranges::views::filter(Element<0>, Element<0>) |
             ranges::views::transform([](const auto &Transition) {
               return TransitionType{Transition.first.second,
                                     Transition.second};
             });
    };
  }

  [[nodiscard]] auto
  propagatedForAcquired(const TypeSetValueType &DerivedType,
                        const TypeSetValueType &BaseType) const {
    return Transitions_.Data |
           ranges::views::filter(EqualTo(DerivedType), ToAcquired) |
           ranges::views::transform(
               [BaseType](const TransitionType &Transition) -> TransitionType {
                 return {std::pair{BaseType, ToRequired(Transition)},
                         Transition.second};
               });
  }

  [[nodiscard]] auto propagatedInheritedMethodsForAcquired(
      const TypeSetValueType &BaseType,
      const TypeSetValueType &DerivedType) const {
    return Transitions_.Data |
           ranges::views::filter(EqualTo(BaseType), ToAcquired) |
           ranges::views::transform(
               [DerivedType](
                   const TransitionType &Transition) -> TransitionType {
                 const auto IsNotConstructor =
                     [](const TransitionDataType &Transition) {
                       return std::visit(
                           Overloaded{
                               [](const clang::FunctionDecl *const FDecl) {
                                 return !llvm::isa<clang::CXXConstructorDecl>(
                                     FDecl);
                               },
                               [](auto &&) { return true; }},
                           Transition);
                     };
                 return {std::pair{DerivedType, ToRequired(Transition)},
                         {ToBundeledTransitionIndex(Transition),
                          ToTransitions(Transition) |
                              ranges::views::filter(
                                  [&DerivedType, &IsNotConstructor](
                                      const TransitionDataType &Transition2) {
                                    return transitionIsMember(DerivedType)(
                                               Transition2) &&
                                           IsNotConstructor(Transition2);
                                  },
                                  Value) |
                              ranges::to<StrippedTransitionsSet>}};
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
  InheritancePropagator(TransitionData &TransitionsRef, DTDGraphData Data)
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

    std::vector<TransitionType> Vec =
        VerticesToVisit |
        ranges::views::filter(ranges::not_fn(Lookup(VertexVisited_))) |
        ranges::views::for_each(PropagateTransitionsOfOutEdges) |
        ranges::to_vector;
    ranges::for_each(Vec, [this](TransitionType NewTransitions) {
      Value(Transitions_.Data[NewTransitions.first])
          .merge(std::move(ToTransitions(NewTransitions)));
    });
  }

private:
  TransitionData &Transitions_;
  DTDGraphData Data_;
  InheritanceGraph Graph_;
  std::vector<bool> VertexVisited_ = std::vector<bool>(Data_.VertexData.size());
};
} // namespace

void propagateInheritance(
    TransitionData &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords) {
  InheritancePropagator{Transitions, createInheritanceGraph(CXXRecords)}();
}

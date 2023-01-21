#include "get_me/direct_type_dependency_propagation.hpp"

#include <type_traits>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/detail/adjacency_list.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <range/v3/action/insert.hpp>
#include <range/v3/action/join.hpp>
#include <range/v3/action/push_back.hpp>
#include <range/v3/action/reverse.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/functional/not_fn.hpp>
#include <range/v3/view/cache1.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/unique.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"
#include "get_me/graph.hpp"
#include "get_me/indexed_graph_sets.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/ranges/functional.hpp"
#include "support/ranges/projections.hpp"
#include "support/ranges/ranges.hpp"
#include "support/ranges/views/conditional.hpp"

enum class DTDEdgeType { Typedef, Inheritance };

using DTDGraphType =
    boost::adjacency_list<boost::listS, boost::vecS, boost::directedS,
                          boost::no_property,
                          boost::property<boost::edge_weight_t, DTDEdgeType>>;

using DTDEdgeDescriptor =
    typename boost::graph_traits<DTDGraphType>::edge_descriptor;
using DTDVertexDescriptor =
    typename boost::graph_traits<DTDGraphType>::vertex_descriptor;

struct DTDDataType {
  using EdgeType = std::pair<DTDVertexDescriptor, DTDVertexDescriptor>;

  std::vector<TypeSetValueType> VertexData{};
  std::vector<EdgeType> Edges{};
  std::vector<DTDEdgeType> EdgeTypes{};
};

class DTDGraphBuilder {
public:
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

  void visit(const clang::CXXRecordDecl *const Record) {
    const auto RecordIndex = addType(launderType(Record->getTypeForDecl()));
    visitCXXRecordDeclImpl(Record, RecordIndex);
  }

  void visitCXXRecordDeclImpl(const clang::CXXRecordDecl *const Derived,
                              const DTDVertexDescriptor DerivedIndex) {
    ranges::for_each(
        Derived->bases(),
        [this, DerivedIndex](const clang::CXXBaseSpecifier &BaseSpec) {
          const auto QType = BaseSpec.getType();
          const auto BaseVertexIndex = addType(launderType(QType.getTypePtr()));

          if (addEdge(DTDDataType::EdgeType{BaseVertexIndex, DerivedIndex},
                      DTDEdgeType::Inheritance)) {
            visitCXXRecordDeclImpl(QType->getAsCXXRecordDecl(),
                                   BaseVertexIndex);
          }
        });
  }

  void visit(const clang::TypedefNameDecl *const Typedef) {
    const auto *const AliasType = launderType(Typedef->getTypeForDecl());
    const auto *const BaseType =
        launderType(Typedef->getUnderlyingType().getTypePtr());
    addEdge(DTDDataType::EdgeType{addType(AliasType), addType(BaseType)},
            DTDEdgeType::Typedef);
  };

  bool addEdge(const DTDDataType::EdgeType &EdgeToAdd, DTDEdgeType Type) {
    const auto EdgesIter = Edges_.lower_bound(EdgeToAdd);
    if (const auto ContainsEdgeToAdd = Value(*EdgesIter) == EdgeToAdd;
        !ContainsEdgeToAdd) {
      Edges_.emplace_hint(EdgesIter, Edges_.size(), EdgeToAdd);
      EdgeTypes_.push_back(Type);
      return true;
    }
    return false;
  }

  [[nodiscard]] DTDDataType getResult() {
    return {getIndexedSetSortedByIndex(std::move(Vertices_)),
            getIndexedSetSortedByIndex(std::move(Edges_)),
            std::move(EdgeTypes_)};
  }

private:
  indexed_set<TypeSetValueType> Vertices_{};
  indexed_set<DTDDataType::EdgeType> Edges_{};
  std::vector<DTDEdgeType> EdgeTypes_{};
};

namespace {
[[nodiscard]] DTDDataType createDTDGraph(
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords,
    const std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls) {
  auto Builder = DTDGraphBuilder{};

  const auto Visitor = [&Builder](const auto *const Value) {
    Builder.visit(Value);
  };

  ranges::for_each(CXXRecords, Visitor);
  ranges::for_each(TypedefNameDecls, Visitor);

  return Builder.getResult();
}

[[nodiscard]] auto getVerticesWithNoInEdges(const DTDDataType &Data) {
  return ranges::views::indices(Data.VertexData.size()) |
         ranges::views::set_difference(Data.Edges |
                                       ranges::views::transform(Element<1>) |
                                       ranges::views::unique);
}

[[nodiscard]] bool overridesMethod(const TypeSetValueType &TypeValue,
                                   const clang::CXXMethodDecl *const Method) {
  return std::visit(
      Overloaded{
          [Method](const clang::Type *const Type) {
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

            const auto IsOveriddenMethod =
                [Method](const clang::CXXMethodDecl *const DerivedMethod) {
                  return DerivedMethod->getNameAsString() ==
                             Method->getNameAsString() &&
                         DerivedMethod->getType() == Method->getType();
                };
            return !ranges::any_of(Derived->methods(), IsOveriddenMethod);
          },
          [](auto &&) { return false; }},
      TypeValue);
}

[[nodiscard]] bool isOverriddenBy(const clang::CXXMethodDecl *const Ctor,
                                  const clang::CXXRecordDecl *const Derived) {
  const auto IsOveriddenCtor =
      [Ctor](const clang::CXXConstructorDecl *const DerivedCtor) {
        return DerivedCtor->getNumParams() == Ctor->getNumParams() &&
               ranges::equal(Ctor->parameters(), DerivedCtor->parameters(),
                             std::less{}, ToQualType, ToQualType);
      };
  return !ranges::any_of(Derived->ctors(), IsOveriddenCtor);
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

[[nodiscard]] std::vector<DTDVertexDescriptor>
getVerticesToVisit(ranges::range auto Sources, const DTDGraphType &Graph) {
  class BFSEdgeCollector : public boost::default_bfs_visitor {
  public:
    explicit BFSEdgeCollector(std::vector<DTDVertexDescriptor> &Collector)
        : Collector_{Collector} {}

    void examine_vertex(DTDVertexDescriptor Vertex,
                        const DTDGraphType & /*Graph*/) {
      Collector_.emplace_back(Vertex);
    }

  private:
    std::vector<DTDVertexDescriptor> &Collector_;
  };

  const auto CollectEdges = [&Graph](const VertexDescriptor Vertex) {
    std::vector<DTDVertexDescriptor> Vertices{};
    boost::breadth_first_search(Graph, Vertex,
                                boost::visitor(BFSEdgeCollector{Vertices}));
    return Vertices;
  };

  return Sources | ranges::views::for_each(CollectEdges) | ranges::to_vector |
         ranges::actions::reverse;
}

[[nodiscard]] DTDGraphType createGraph(const DTDDataType &DTDData) {
  return {DTDData.Edges.data(), DTDData.Edges.data() + DTDData.Edges.size(),
          DTDData.EdgeTypes.data(), DTDData.EdgeTypes.size()};
}
} // namespace

class DepthFirstDTDPropagation {
private:
  [[nodiscard]] auto toEdgeTypeFactory() const {
    return [WheightMap = boost::get(boost::edge_weight, Graph_)](
               const EdgeDescriptor &Edge) { return get(WheightMap, Edge); };
  }

  [[nodiscard]] const TypeSetValueType &
  toType(const VertexDescriptor Vertex) const {
    return Data_.VertexData[Vertex];
  }

  template <typename... Ts>
  [[nodiscard]] auto propagate(Ts &&...Propagators)
    requires(std::is_invocable_v<Ts, DTDEdgeDescriptor> && ...)
  {
    return ranges::views::for_each(
        [... Propagators =
             std::forward<Ts>(Propagators)](const DTDEdgeDescriptor &Edge) {
          return ranges::views::concat(Propagators(Edge)...);
        });
  }

  [[nodiscard]] auto propagateRequired() const {
    return [this](const DTDEdgeDescriptor &Edge) {
      return Transitions_ |
             ranges::views::transform(
                 [SourceType = toType(Source(Edge)),
                  TargetType = toType(Target(Edge))](
                     const BundeledTransitionType &BundeledTransition)
                     -> BundeledTransitionType {
                   return {ToAcquired(BundeledTransition),
                           BundeledTransition.second |
                               ranges::views::transform(
                                   maybePropagate(SourceType, TargetType)) |
                               ranges::views::cache1 |
                               ranges::views::filter(Element<0>) |
                               ranges::views::transform(Element<1>) |
                               ranges::to<StrippedTransitionsSet>};
                 });
    };
  }

  [[nodiscard]] auto
  propagatedForAcquired(const bool Enabled, const TypeSetValueType &DerivedType,
                        const TypeSetValueType &BaseType) const {
    return Transitions_ | Conditional(Enabled) |
           ranges::views::filter(EqualTo(DerivedType), ToAcquired) |
           ranges::views::transform(
               [BaseType](const BundeledTransitionType &BundeledTransition)
                   -> BundeledTransitionType {
                 return {BaseType, BundeledTransition.second};
               });
  }

  [[nodiscard]] auto propagatedInheritedMethodsForAcquired(
      const bool Enabled, const TypeSetValueType &BaseType,
      const TypeSetValueType &DerivedType) const {
    return Transitions_ | Conditional(Enabled) |
           ranges::views::filter(EqualTo(BaseType), ToAcquired) |
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
    return [this,
            ToEdgeType = toEdgeTypeFactory()](const DTDEdgeDescriptor &Edge) {
      if (const auto EdgeType = ToEdgeType(Edge);
          EdgeType != DTDEdgeType::Inheritance) {
        return ranges::views::concat(
            propagatedInheritedMethodsForAcquired(false, {}, {}),
            propagatedForAcquired(false, {}, {}));
      }

      const auto &BaseType = toType(Source(Edge));
      const auto &DerivedType = toType(Target(Edge));

      return ranges::views::concat(
          propagatedInheritedMethodsForAcquired(true, BaseType, DerivedType),
          propagatedForAcquired(true, DerivedType, BaseType));
    };
  }

  [[nodiscard]] auto propagateAcquiredTypedef() const {
    return [this,
            ToEdgeType = toEdgeTypeFactory()](const DTDEdgeDescriptor &Edge) {
      if (const auto EdgeType = ToEdgeType(Edge);
          EdgeType != DTDEdgeType::Typedef) {
        return ranges::views::concat(propagatedForAcquired(false, {}, {}),
                                     propagatedForAcquired(false, {}, {}));
      }

      const auto &SourceType = toType(Source(Edge));
      const auto &TargetType = toType(Target(Edge));

      return ranges::views::concat(
          propagatedForAcquired(true, SourceType, TargetType),
          propagatedForAcquired(true, TargetType, SourceType));
    };
  }

public:
  DepthFirstDTDPropagation(TransitionCollector &TransitionsRef,
                           DTDDataType Data)
      : Transitions_{TransitionsRef},
        Data_{std::move(Data)},
        Graph_{createGraph(Data_)} {}

  void operator()() {
    const auto Sources = getVerticesWithNoInEdges(Data_);
    const auto VerticesToVisit = getVerticesToVisit(Sources, Graph_);

    const auto PropagateTransitionsOfOutEdges =
        [this](const DTDVertexDescriptor Vertex) {
          VertexVisited_[Vertex] = true;
          return toRange(boost::out_edges(Vertex, Graph_)) |
                 propagate(propagateRequired(), propagateAcquiredInheritance(),
                           propagateAcquiredTypedef());
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
  TransitionCollector EmptyTransitions_{};
  DTDDataType Data_;
  DTDGraphType Graph_;
  std::vector<bool> VertexVisited_ = std::vector<bool>(Data_.VertexData.size());
};

void propagateTransitionsOfDirectTypeDependencies(
    TransitionCollector &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords,
    const std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls) {
  DepthFirstDTDPropagation{Transitions,
                           createDTDGraph(CXXRecords, TypedefNameDecls)}();
}

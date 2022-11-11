#include "get_me/direct_type_dependency_propagation.hpp"

#include <type_traits>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <range/v3/action/reverse.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/cache1.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/unique.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"
#include "get_me/indexed_graph_sets.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "get_me/utility.hpp"

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
  [[nodiscard]] std::pair<bool, VertexDescriptor>
  addType(const TypeSetValueType &Type) {
    const auto BaseTypeIter = Vertices.find(Type);
    const auto BaseTypeExists = BaseTypeIter != Vertices.end();
    const auto BaseVertexIndex =
        BaseTypeExists ? Index(*BaseTypeIter) : Vertices.size();
    if (!BaseTypeExists) {
      Vertices.emplace(BaseVertexIndex, Type);
    }
    return {BaseTypeExists, BaseVertexIndex};
  }

  void visitCXXRecordDecl(const clang::CXXRecordDecl *const Derived,
                          const DTDVertexDescriptor DerivedIndex) {
    ranges::for_each(
        Derived->bases(),
        [this, &DerivedIndex](const clang::CXXBaseSpecifier &BaseSpec) {
          const auto QType = BaseSpec.getType();
          const auto BaseType =
              TypeSetValueType{launderType(QType.getTypePtr())};
          const auto [BaseVertexExists, BaseVertexIndex] = addType(BaseType);

          const auto EdgeToAdd =
              DTDDataType::EdgeType{BaseVertexIndex, DerivedIndex};
          if (Edges.contains(EdgeToAdd)) {
            return;
          }
          Edges.emplace(Edges.size(), EdgeToAdd);
          EdgeTypes.push_back(DTDEdgeType::Inheritance);
          visitCXXRecordDecl(QType->getAsCXXRecordDecl(), BaseVertexIndex);
        });
  };

  void visitTypedefNameDecl(const clang::TypedefNameDecl *const Typedef) {
    const auto *const AliasType = launderType(Typedef->getTypeForDecl());
    const auto *const BaseType =
        launderType(Typedef->getUnderlyingType().getTypePtr());
    const auto EdgeToAdd = DTDDataType::EdgeType{addType(AliasType).second,
                                                 addType(BaseType).second};
    const auto EdgesIter = Edges.lower_bound(EdgeToAdd);
    if (const auto ContainsEdgeToAdd = Value(*EdgesIter) == EdgeToAdd;
        !ContainsEdgeToAdd) {
      Edges.emplace_hint(EdgesIter, Edges.size(), EdgeToAdd);
      EdgeTypes.push_back(DTDEdgeType::Typedef);
    }
  };

  [[nodiscard]] DTDDataType getResult() {
    return {getIndexedSetSortedByIndex(std::move(Vertices)),
            getIndexedSetSortedByIndex(std::move(Edges)), std::move(EdgeTypes)};
  }

private:
  indexed_set<TypeSetValueType> Vertices{};
  indexed_set<DTDDataType::EdgeType> Edges{};
  std::vector<DTDEdgeType> EdgeTypes{};
};

[[nodiscard]] static std::pair<DTDDataType, DTDGraphType> createDTDGraph(
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords,
    const std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls) {
  auto Builder = DTDGraphBuilder{};

  ranges::for_each(CXXRecords, [&Builder](
                                   const clang::CXXRecordDecl *const Record) {
    const auto RecordIndex =
        Builder.addType(TypeSetValueType{launderType(Record->getTypeForDecl())})
            .second;
    Builder.visitCXXRecordDecl(Record, RecordIndex);
  });

  ranges::for_each(TypedefNameDecls,
                   [&Builder](const clang::TypedefNameDecl *const Typedef) {
                     Builder.visitTypedefNameDecl(Typedef);
                   });

  auto DTDData = Builder.getResult();
  return std::pair{DTDData,
                   DTDGraphType{DTDData.Edges.data(),
                                DTDData.Edges.data() + DTDData.Edges.size(),
                                DTDData.EdgeTypes.data(),
                                DTDData.EdgeTypes.size()}};
}

[[nodiscard]] std::vector<DTDVertexDescriptor>
getVerticesToVisit(ranges::range auto Sources, const DTDGraphType &Graph) {
  class BFSEdgeCollector : public boost::default_bfs_visitor {
  public:
    explicit BFSEdgeCollector(std::vector<DTDVertexDescriptor> &Collector)
        : Collector{Collector} {}

    void examine_vertex(DTDVertexDescriptor Vertex,
                        const DTDGraphType & /*Graph*/) {
      Collector.emplace_back(Vertex);
    }

  private:
    std::vector<DTDVertexDescriptor> &Collector;
  };

  std::vector<DTDVertexDescriptor> Vertices{};
  auto Visitor = boost::visitor(BFSEdgeCollector{Vertices});
  ranges::for_each(Sources, [&Visitor, &Graph](const VertexDescriptor Vertex) {
    boost::breadth_first_search(Graph, Vertex, Visitor);
  });
  return std::move(Vertices) | ranges::actions::reverse;
}

template <typename T> [[nodiscard]] auto invokePropagator(T &&Propagator) {
  return ranges::views::transform(
      [Propagator = std::forward<T>(Propagator)](TransitionType Transition) {
        auto PropagationResult = Propagator(Transition);
        return std::tuple{std::move(Transition), std::move(PropagationResult)};
      });
}

[[nodiscard]] auto propagateRequiredImpl(TypeSetValueType TargetType,
                                         auto PropagateRequired) {
  return invokePropagator(std::move(PropagateRequired)) |
         ranges::views::cache1 |
         ranges::views::filter([](const auto &TransitionAndPropagationStatus) {
           return std::get<1>(TransitionAndPropagationStatus).second;
         }) |
         ranges::views::transform(
             [TargetType](
                 auto TransitionAndPropagationStatus) -> TransitionType {
               auto &[Transition, PropagateRequiredStatus] =
                   TransitionAndPropagationStatus;
               auto &Required = std::get<2>(Transition);
               Required.erase(std::get<0>(PropagateRequiredStatus));
               Required.insert(TargetType);
               return {acquired(Transition), transition(Transition), Required};
             });
}

[[nodiscard]] auto propagateAcquiredImpl(TypeSetValueType TargetType,
                                         auto PropagateAcquired) {
  return invokePropagator(std::move(PropagateAcquired)) |
         ranges::views::cache1 |
         ranges::views::filter([](const auto &TransitionAndPropagationStatus) {
           return std::get<1>(TransitionAndPropagationStatus);
         }) |
         ranges::views::transform(
             [TargetType](
                 const auto &TransitionAndPropagationStatus) -> TransitionType {
               const auto &Transition =
                   std::get<0>(TransitionAndPropagationStatus);
               return {{TypeSet{TargetType}},
                       transition(Transition),
                       required(Transition)};
             });
}

[[nodiscard]] static auto getVerticesWithNoInEdges(const DTDDataType &Data) {
  return ranges::views::iota(0U, Data.VertexData.size()) |
         ranges::views::set_difference(Data.Edges |
                                       ranges::views::transform(Element<1>) |
                                       ranges::views::unique);
}

[[nodiscard]] static bool
overridesMethod(const TypeSetValueType &Type,
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
      Type);
}

[[nodiscard]] static bool
isOverriddenBy(const clang::CXXMethodDecl *const Ctor,
               const clang::CXXRecordDecl *const Derived) {
  const auto IsOveriddenCtor =
      [Ctor](const clang::CXXConstructorDecl *const DerivedCtor) {
        return DerivedCtor->getNumParams() == Ctor->getNumParams() &&
               ranges::equal(Ctor->parameters(), DerivedCtor->parameters(),
                             std::less{}, ToQualType, ToQualType);
      };
  return !ranges::any_of(Derived->ctors(), IsOveriddenCtor);
}

[[nodiscard]] static bool
overridesConstructor(const TypeSetValueType &Type,
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
      Type);
}

class DepthFirstDTDPropagation {
private:
  [[nodiscard]] auto toEdgeTypeFactory() const {
    return [WheightMap = boost::get(boost::edge_weight, Graph)](
               const EdgeDescriptor &Edge) { return get(WheightMap, Edge); };
  }

  template <typename... Ts>
  [[nodiscard]] auto propagate(Ts &&...Propagators)
    requires(
        std::is_invocable_r_v<TransitionCollector, Ts, DTDEdgeDescriptor> &&
        ...)
  {
    return ranges::views::for_each(
        [... Propagators = std::forward<Ts>(Propagators)](
            const DTDEdgeDescriptor &Edge) -> std::vector<TransitionCollector> {
          return {Propagators(Edge)...};
        });
  }

  [[nodiscard]] auto propagateRequired() {
    return [this](const DTDEdgeDescriptor &Edge) {
      const auto SourceType = Data.VertexData[Edge.m_source];
      const auto TargetType = Data.VertexData[Edge.m_target];

      const auto PropagateRequired =
          [&SourceType](const TransitionType &Transition) {
            const auto &Required = required(Transition);
            const auto Iter = Required.find(SourceType);
            return std::pair{Iter, Iter != Required.end()};
          };

      return Transitions |
             propagateRequiredImpl(TargetType, PropagateRequired) |
             ranges::to<TransitionCollector>;
    };
  }

  [[nodiscard]] auto generatePropagatedTransitionsForAcquired(
      const TypeSetValueType &OldType, const TypeSetValueType &NewType) const {
    const auto PropagateAcquired =
        [&OldType](const TransitionType &Transition) {
          return acquired(Transition).contains(OldType);
        };
    // FIXME: apply iterating optimization if possible
    return Transitions | propagateAcquiredImpl(NewType, PropagateAcquired);
  }

  [[nodiscard]] auto getPropagatedTransitionsForInheritedMethodsForAcquired(
      const TypeSetValueType &OldType, const TypeSetValueType &NewType) {
    const auto PropagateAcquired = [&OldType, &NewType](
                                       const TransitionType &Transition) {
      const auto TransitionIsFromRecord = std::visit(
          Overloaded{[](const clang::FieldDecl *const) { return false; },
                     [&NewType](const clang::FunctionDecl *const FDecl) {
                       const auto *const Method =
                           llvm::dyn_cast<clang::CXXMethodDecl>(FDecl);
                       if (Method == nullptr) {
                         return false;
                       }
                       if (const auto *const Ctor =
                               llvm::dyn_cast<clang::CXXConstructorDecl>(
                                   Method);
                           Ctor != nullptr) {
                         return overridesConstructor(NewType, Ctor);
                       }

                       return overridesMethod(NewType, Method);
                     },
                     [](const auto *const) { return false; }},
          transition(Transition));

      return acquired(Transition).contains(OldType) && TransitionIsFromRecord;
    };
    return Transitions | propagateAcquiredImpl(NewType, PropagateAcquired);
  };

  [[nodiscard]] auto propagateAcquired() {
    return [this, ToEdgeType = toEdgeTypeFactory()](
               const DTDEdgeDescriptor &Edge) -> TransitionCollector {
      const auto &SourceType = Data.VertexData[Edge.m_source];
      const auto &TargetType = Data.VertexData[Edge.m_target];

      switch (const auto EdgeType = ToEdgeType(Edge); EdgeType) {
      case DTDEdgeType::Inheritance:
        return ranges::views::concat(
                   getPropagatedTransitionsForInheritedMethodsForAcquired(
                       SourceType, TargetType),
                   generatePropagatedTransitionsForAcquired(TargetType,
                                                            SourceType)) |
               ranges::to<TransitionCollector>;
      case DTDEdgeType::Typedef:
        return ranges::views::concat(generatePropagatedTransitionsForAcquired(
                                         SourceType, TargetType),
                                     generatePropagatedTransitionsForAcquired(
                                         TargetType, SourceType)) |
               ranges::to<TransitionCollector>;
      }
      return {};
    };
  }

  [[nodiscard]] auto getOutEdges() const {
    return ranges::views::iota(0U, Data.VertexData.size()) |
           ranges::views::transform([this](const VertexDescriptor Vertex) {
             return toRange(out_edges(Vertex, Graph));
           });
  }

public:
  DepthFirstDTDPropagation(TransitionCollector &TransitionsRef,
                           const DTDDataType &DataRef,
                           const DTDGraphType &GraphRef)
      : Transitions{TransitionsRef}, Data{DataRef}, Graph{GraphRef} {}
  void operator()() {
    const auto Sources = getVerticesWithNoInEdges(Data);
    const auto OutEdges = getOutEdges();
    const auto VerticesToVisit = getVerticesToVisit(Sources, Graph);
    const auto NotVisited = [this](const DTDVertexDescriptor Vertex) {
      return !VertexVisited[Vertex];
    };

    const auto ToPropagatedTransitionsOfVertex =
        [this, &OutEdges](const DTDVertexDescriptor Vertex) {
          VertexVisited[Vertex] = true;
          return OutEdges[Vertex] |
                 propagate(propagateAcquired(), propagateRequired()) |
                 ranges::views::join | ranges::to<TransitionCollector>;
        };
    ranges::for_each(
        VerticesToVisit | ranges::views::filter(NotVisited) |
            ranges::views::transform(ToPropagatedTransitionsOfVertex),
        [this](TransitionCollector NewTransitions) {
          Transitions.merge(NewTransitions);
        });
  }

private:
  TransitionCollector &Transitions;
  const DTDDataType &Data;
  const DTDGraphType &Graph;
  std::vector<bool> VertexVisited = std::vector<bool>(Data.VertexData.size());
};

// FIXME: swap order of graph?
// I.e. from most basic type to derived

void propagateTransitionsOfDirectTypeDependencies(
    TransitionCollector &Transitions,
    const std::vector<const clang::CXXRecordDecl *> &CXXRecords,
    const std::vector<const clang::TypedefNameDecl *> &TypedefNameDecls) {
  auto [Data, Graph] = createDTDGraph(CXXRecords, TypedefNameDecls);
  // spdlog::info("TypedefNameDecls: {}", TypedefNameDecls.size());
  // spdlog::info("CXXRecords: {}", CXXRecords.size());
  // spdlog::info("Transitions: {}", Transitions);
  // spdlog::info("Data.VertexData: {}",
  //              ranges::views::zip(ranges::views::iota(0U),
  //              Data.VertexData));
  // spdlog::info("Data.Edges: {}", Data.Edges);
  DepthFirstDTDPropagation{Transitions, Data, Graph}();
}

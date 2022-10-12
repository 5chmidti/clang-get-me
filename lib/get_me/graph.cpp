#include "get_me/graph.hpp"

#include <iterator>
#include <string>
#include <utility>

#include <boost/algorithm/string.hpp>
#include <boost/graph/detail/adjacency_list.hpp>
#include <boost/graph/detail/edge.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/property_map/property_map.hpp>
#include <clang/AST/DeclCXX.h>
#include <fmt/chrono.h>
#include <llvm/Support/Casting.h>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/subrange.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"
#include "get_me/path_traversal.hpp"
#include "get_me/type_set.hpp"

[[nodiscard]] static auto matchesNamePredicateFactory(std::string Name) {
  return [Name = std::move(Name)](const TypeSetValueType &Val) {
    return std::visit(
        Overloaded{
            [&Name](const clang::Type *const Type) {
              const auto QType = clang::QualType(Type, 0);
              const auto TypeAsString = [&QType]() {
                auto QTypeAsString = QType.getAsString();
                boost::erase_all(QTypeAsString, "struct");
                boost::erase_all(QTypeAsString, "class");
                boost::trim(QTypeAsString);
                return QTypeAsString;
              }();
              const auto EquivalentName = TypeAsString == Name;
              if (!EquivalentName &&
                  (TypeAsString.find(Name) != std::string::npos)) {
                spdlog::trace(
                    "matchesName(QualType): no match for close match: {} vs {}",
                    TypeAsString, Name);
              }
              return EquivalentName;
            },
            [](const ArithmeticType &) { return false; }},
        Val);
  };
}

static void initializeVertexDataWithQueried(
    const TransitionCollector &TypeSetTransitionData, GraphData &Data,
    const std::string &TypeName) {
  const auto ToAcquired = [](const TransitionType &Val) {
    return std::get<0>(Val);
  };
  const auto MatchesQueriedName = matchesNamePredicateFactory(TypeName);
  ranges::transform(ranges::views::filter(
                        TypeSetTransitionData,
                        [&Data, &MatchesQueriedName](const TypeSet &Acquired) {
                          return ranges::any_of(Acquired, MatchesQueriedName) &&
                                 !ranges::contains(Data.VertexData, Acquired);
                        },
                        ToAcquired),
                    std::back_inserter(Data.VertexData), ToAcquired);
}

[[nodiscard]] static auto isSubsetPredicateFactory(const TypeSet &Subset) {
  return
      [&Subset](const TypeSet &Superset) { return isSubset(Superset, Subset); };
}

[[nodiscard]] static TypeSet merge(TypeSet Lhs, TypeSet Rhs) {
  Lhs.merge(std::move(Rhs));
  return Lhs;
}

[[nodiscard]] static TypeSet subtract(const TypeSet &Lhs, const TypeSet &Rhs) {
  TypeSet Res{};
  ranges::set_difference(Lhs, Rhs, std::inserter(Res, Res.end()), std::less{});
  return Res;
}

using indexed_vertex_type = std::pair<TypeSet, size_t>;
struct VertexSetComparator {
  using is_transparent = void;
  [[nodiscard]] bool operator()(const indexed_vertex_type &Lhs,
                                const indexed_vertex_type &Rhs) const {
    return Lhs.first < Rhs.first;
  }
  [[nodiscard]] bool operator()(const indexed_vertex_type &Lhs,
                                const TypeSet &Rhs) const {
    return Lhs.first < Rhs;
  }
  [[nodiscard]] bool operator()(const TypeSet &Lhs,
                                const indexed_vertex_type &Rhs) const {
    return Lhs < Rhs.first;
  }
};
using vertex_set = std::set<indexed_vertex_type, VertexSetComparator>;

using indexed_edge_type = std::pair<GraphData::EdgeType, size_t>;
struct EdgeSetComparator {
  using is_transparent = void;
  [[nodiscard]] bool operator()(const indexed_edge_type &Lhs,
                                const indexed_edge_type &Rhs) const {
    return Lhs < Rhs;
  }
  [[nodiscard]] bool operator()(const indexed_edge_type &Lhs,
                                const GraphData::EdgeType &Rhs) const {
    return Lhs.first < Rhs;
  }
  [[nodiscard]] bool operator()(const GraphData::EdgeType &Lhs,
                                const indexed_edge_type &Rhs) const {
    return Lhs < Rhs.first;
  }
};
using edge_set = std::set<indexed_edge_type, EdgeSetComparator>;

[[nodiscard]] static bool edgeWithTransitionExistsInContainer(
    const edge_set &Edges, const GraphData::EdgeType &EdgeToAdd,
    const TransitionType &Transition,
    const std::vector<GraphData::EdgeWeightType> &EdgeWeights) {
  const auto LowerBound = Edges.lower_bound(EdgeToAdd);
  const auto UpperBound = Edges.upper_bound(EdgeToAdd);
  return ranges::contains(
      ranges::subrange(LowerBound, UpperBound) |
          ranges::views::transform(
              [&EdgeWeights](const indexed_edge_type &IndexedEdge) {
                return EdgeWeights[IndexedEdge.second];
              }),
      Transition);
}

[[nodiscard]] static bool setIntersectionIsEmpty(const TypeSet &Lhs,
                                                 const TypeSet &Rhs) {
  return ranges::all_of(Lhs, [&Rhs](const TypeSetValueType &LhsElement) {
    const auto Lower = Rhs.lower_bound(LhsElement);
    const auto Upper = Rhs.upper_bound(LhsElement);
    return Lower == Upper;
  });
}

[[nodiscard]] static bool independent(const TransitionType &Lhs,
                                      const TransitionType &Rhs) {
  return setIntersectionIsEmpty(std::get<0>(Lhs), std::get<2>(Rhs)) &&
         setIntersectionIsEmpty(std::get<2>(Lhs), std::get<0>(Rhs));
}

[[nodiscard]] static auto addTransitionToIndependentTransitionsOfEdgeFactory(
    const TransitionType &Transition) {
  return [&Transition](auto VertexAndTransitionsPair) {
    if (auto &[_, IndependentTransitions] = VertexAndTransitionsPair;
        ranges::all_of(
            IndependentTransitions,
            [&Transition](const TransitionType &IndependentTransition) {
              return independent(IndependentTransition, Transition);
            })) {
      IndependentTransitions.push_back(Transition);
    }
  };
}

[[nodiscard]] static auto constructVertexAndTransitionsPairVector(
    const vertex_set &InterestingVertices,
    const TransitionCollector &Transitions) {
  auto IndependentTransitionsVec =
      std::vector<std::vector<TransitionType>>(InterestingVertices.size());

  ranges::for_each(
      Transitions, [&InterestingVertices, &IndependentTransitionsVec](
                       const TransitionType &Transition) {
        ranges::for_each(
            ranges::views::zip(InterestingVertices, IndependentTransitionsVec) |
                ranges::views::filter(
                    [&Transition](const auto &VertexAndTransitionsPair) {
                      return isSubset(VertexAndTransitionsPair.first.first,
                                      std::get<0>(Transition));
                    }),
            addTransitionToIndependentTransitionsOfEdgeFactory(Transition));
      });

  return ranges::to_vector(ranges::views::zip(
      InterestingVertices, ranges::views::move(IndependentTransitionsVec)));
}

static void buildGraph(const TransitionCollector &TypeSetTransitionData2,
                       GraphData &Data, const Config &Conf) {
  const auto QueriedTypes = Data.VertexData;
  // FIXME: do the filtering in tooling
  const auto TypeSetTransitionData = ranges::to<TransitionCollector>(
      TypeSetTransitionData2 |
      ranges::views::filter([&QueriedTypes](const TransitionType &Transition) {
        return !ranges::any_of(
            QueriedTypes, [&Transition](const auto &QueriedType) {
              return isSubset(std::get<2>(Transition), QueriedType);
            });
      }));

  auto VertexData = ranges::to<vertex_set>(ranges::views::zip(
      Data.VertexData, ranges::views::iota(static_cast<size_t>(0U))));

  edge_set EdgesData{};

  size_t IterationCount = 0U;

  auto InterstingVertices = VertexData;
  auto NewInterstingVertices = vertex_set{};

  Data.VertexData.emplace_back();

  const auto ToTransitionAndTargetTypeSetPairForVertex =
      [](const indexed_vertex_type &IndexedVertex) {
        return [&IndexedVertex](const TransitionType &Transition) {
          return std::pair{Transition, merge(subtract(IndexedVertex.first,
                                                      std::get<0>(Transition)),
                                             std::get<2>(Transition))};
        };
      };

  for (bool AddedTransitions = true;
       AddedTransitions && IterationCount < Conf.MaxGraphDepth;
       ++IterationCount) {
    AddedTransitions = false;
    for (auto [IndexedVertex, Transitions] :
         constructVertexAndTransitionsPairVector(InterstingVertices,
                                                 TypeSetTransitionData)) {
      const auto SourceVertexIndex = IndexedVertex.second;

      for (const auto &[Transition, TargetTypeSet] :
           Transitions |
               ranges::views::transform(
                   ToTransitionAndTargetTypeSetPairForVertex(IndexedVertex))) {
        const auto TargetVertexIter = VertexData.find(TargetTypeSet);
        const auto TargetVertexExists = TargetVertexIter != VertexData.end();
        const auto TargetVertexIndex =
            TargetVertexExists ? TargetVertexIter->second : VertexData.size();

        const auto EdgeToAdd =
            GraphData::EdgeType{SourceVertexIndex, TargetVertexIndex};

        if (TargetVertexExists &&
            edgeWithTransitionExistsInContainer(EdgesData, EdgeToAdd,
                                                Transition, Data.EdgeWeights)) {
          continue;
        }

        if (TargetVertexExists) {
          NewInterstingVertices.emplace(*TargetVertexIter);
        } else {
          NewInterstingVertices.emplace(TargetTypeSet, TargetVertexIndex);
          VertexData.emplace(TargetTypeSet, TargetVertexIndex);
        }
        if (const auto [_, EdgeAdded] =
                EdgesData.emplace(EdgeToAdd, EdgesData.size());
            EdgeAdded) {
          Data.EdgeWeights.push_back(Transition);
          AddedTransitions = true;
        }
      }
    }

    spdlog::trace("#{} |V| = {}, |E| = {}", IterationCount, VertexData.size(),
                  EdgesData.size());

    InterstingVertices = std::move(NewInterstingVertices);
    NewInterstingVertices.clear();
  }
  spdlog::trace("{:=^50}", "");

  const auto GetSortedIndexedData =
      []<ranges::range RangeType>(const RangeType &Range) {
        auto DataToSort = ranges::to_vector(Range);
        ranges::sort(DataToSort, std::less{},
                     &ranges::range_value_t<RangeType>::second);
        return DataToSort;
      };

  auto SortedVertexData = GetSortedIndexedData(VertexData);
  Data.VertexData = ranges::to_vector(
      SortedVertexData | ranges::views::transform(&indexed_vertex_type::first));

  auto SortedEdgeData = GetSortedIndexedData(EdgesData);
  Data.Edges = ranges::to_vector(
      SortedEdgeData | ranges::views::transform(&indexed_edge_type::first));

  Data.EdgeIndices = ranges::to_vector(
      ranges::views::iota(static_cast<size_t>(0U), Data.Edges.size()));
}

std::pair<GraphType, GraphData>
createGraph(const TransitionCollector &TypeSetTransitionData,
            const std::string &TypeName, const Config &Conf) {
  GraphData Data{};
  initializeVertexDataWithQueried(TypeSetTransitionData, Data, TypeName);

  spdlog::trace("initial GraphData.VertexData: {}", Data.VertexData);

  buildGraph(TypeSetTransitionData, Data, Conf);

  return {GraphType(Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
                    Data.EdgeIndices.data(), Data.EdgeIndices.size()),
          Data};
}

std::optional<VertexDescriptor>
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const std::string &TypeName) {
  // FIXME: improve queried type matching:
  // - allow matching mutiple to get around QualType vs NamedDecl problem
  // FIXME: only getting the 'A' type, not the & qualified
  const auto SourceVertex =
      ranges::find_if(Data.VertexData, [&TypeName](const TypeSet &TSet) {
        return TSet.end() !=
               ranges::find_if(TSet, matchesNamePredicateFactory(TypeName));
      });

  if (SourceVertex == Data.VertexData.end()) {
    spdlog::error("found no type matching {} in {}", TypeName, Data.VertexData);
    return std::nullopt;
  }
  return static_cast<VertexDescriptor>(
      std::distance(Data.VertexData.begin(), SourceVertex));
}

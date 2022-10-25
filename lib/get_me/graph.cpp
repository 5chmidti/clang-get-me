#include "get_me/graph.hpp"

#include <deque>
#include <exception>
#include <functional>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/container/flat_set.hpp>
#include <clang/AST/Type.h>
#include <fmt/core.h>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/subrange.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "get_me/indexed_graph_sets.hpp"
#include "get_me/type_set.hpp"
#include "get_me/utility.hpp"

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
  const auto MatchesQueriedName = matchesNamePredicateFactory(TypeName);
  ranges::transform(ranges::views::filter(
                        TypeSetTransitionData,
                        [&Data, &MatchesQueriedName](const TypeSet &Acquired) {
                          return ranges::any_of(Acquired, MatchesQueriedName) &&
                                 !ranges::contains(Data.VertexData, Acquired);
                        },
                        acquired),
                    std::back_inserter(Data.VertexData), acquired);
}

[[nodiscard]] static bool edgeWithTransitionExistsInContainer(
    const indexed_set<GraphData::EdgeType> &Edges,
    const GraphData::EdgeType &EdgeToAdd, const TransitionType &Transition,
    const std::vector<GraphData::EdgeWeightType> &EdgeWeights) {
  return ranges::contains(
      ranges::subrange(Edges.lower_bound(EdgeToAdd),
                       Edges.upper_bound(EdgeToAdd)) |
          ranges::views::transform(
              [&EdgeWeights](
                  const indexed_value_type<GraphData::EdgeType> &IndexedEdge) {
                return EdgeWeights[IndexedEdge.second];
              }),
      Transition);
}

[[nodiscard]] static auto constructVertexAndTransitionsPairVector(
    const indexed_set<TypeSet> &InterestingVertices,
    const TransitionCollector &Transitions) {
  auto IndependentTransitionsVec =
      std::vector<std::vector<TransitionType>>(InterestingVertices.size());

  ranges::for_each(
      Transitions, [&InterestingVertices, &IndependentTransitionsVec](
                       const TransitionType &Transition) {
        const auto AcquiredIsSubset =
            [Acquired = acquired(Transition)](const auto &IndexedVertex) {
              return isSubset(IndexedVertex.first, Acquired);
            };
        const auto IndependentOfTransition =
            [&Transition](const TransitionType &IndependentTransition) {
              return independent(IndependentTransition, Transition);
            };
        const auto AllAreIndependentOfTransition =
            [&IndependentOfTransition](
                const std::vector<TransitionType> &IndependentTransitions) {
              return ranges::all_of(IndependentTransitions,
                                    IndependentOfTransition);
            };
        const auto AddToIndependentTransitionsOfEdge =
            [&Transition](std::vector<TransitionType> &IndependentTransitions) {
              IndependentTransitions.push_back(Transition);
            };
        ranges::for_each(
            ranges::views::zip(InterestingVertices, IndependentTransitionsVec) |
                ranges::views::filter(AcquiredIsSubset, Element<0>) |
                ranges::views::transform(Element<1>) |
                ranges::views::filter(AllAreIndependentOfTransition),
            AddToIndependentTransitionsOfEdge);
      });

  return ranges::views::zip(InterestingVertices,
                            ranges::views::move(IndependentTransitionsVec)) |
         ranges::to_vector;
}

static void buildGraph(const TransitionCollector &TypeSetTransitionData2,
                       GraphData &Data, const Config &Conf) {
  const auto QueriedTypes = Data.VertexData;
  // FIXME: do the filtering in tooling
  const auto DoesNotRequireQueriedType =
      [&QueriedTypes](const TypeSet &Required) {
        const auto IsSubsetOfRequired = [&Required](const auto &QueriedType) {
          return isSubset(Required, QueriedType);
        };
        return !ranges::any_of(QueriedTypes, IsSubsetOfRequired);
      };
  const auto TypeSetTransitionData =
      TypeSetTransitionData2 |
      ranges::views::filter(DoesNotRequireQueriedType, required) |
      ranges::to<TransitionCollector>;

  auto VertexData = ranges::to<indexed_set<TypeSet>>(ranges::views::zip(
      Data.VertexData, ranges::views::iota(static_cast<size_t>(0U))));

  indexed_set<GraphData::EdgeType> EdgesData{};

  size_t IterationCount = 0U;

  auto InterstingVertices = VertexData;
  auto NewInterstingVertices = indexed_set<TypeSet>{};

  Data.VertexData.emplace_back();

  const auto ToTransitionAndTargetTypeSetPairForVertex =
      [](const indexed_value_type<TypeSet> &IndexedVertex) {
        return [&IndexedVertex](const TransitionType &Transition) {
          return std::pair{Transition,
                           ranges::views::set_union(
                               ranges::views::set_difference(
                                   IndexedVertex.first, acquired(Transition)),
                               required(Transition)) |
                               ranges::to<TypeSet>};
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

  Data.VertexData = getIndexedSetSortedByIndex(std::move(VertexData));
  Data.Edges = getIndexedSetSortedByIndex(std::move(EdgesData));
  Data.EdgeIndices =
      ranges::views::iota(static_cast<size_t>(0U), Data.Edges.size()) |
      ranges::to_vector;
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

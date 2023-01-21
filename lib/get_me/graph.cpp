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
#include <range/v3/action/push_back.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/fold_left.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/subrange.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "get_me/indexed_graph_sets.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/functional.hpp"

bool edgeWithTransitionExistsInContainer(
    const indexed_set<GraphData::EdgeType> &Edges,
    const GraphData::EdgeType &EdgeToAdd, const TransitionType &Transition,
    const std::vector<GraphData::EdgeWeightType> &EdgeWeights) {
  return ranges::contains(
      ranges::subrange(Edges.lower_bound(EdgeToAdd),
                       Edges.upper_bound(EdgeToAdd)) |
          ranges::views::transform(Lookup(EdgeWeights, Index)),
      Transition);
}

namespace {
[[nodiscard]] std::vector<
    std::pair<GraphBuilder::VertexSet::value_type, std::vector<TransitionType>>>
constructVertexAndTransitionsPairVector(
    GraphBuilder::VertexSet InterestingVertices,
    const TransitionCollector &Transitions) {
  auto IndependentTransitionsVec = ranges::fold_left(
      Transitions,
      std::vector<std::vector<TransitionType>>(InterestingVertices.size()),
      [&InterestingVertices](auto CurrentIndependentTransitionsVec,
                             const BundeledTransitionType &BundeledTransition) {
        const auto MaybeAddIndependentTransitions =
            [Acquired = ToAcquired(BundeledTransition), &InterestingVertices,
             &CurrentIndependentTransitionsVec](
                const StrippedTransitionType &StrippedTransition) {
              const auto Transition =
                  TransitionType{Acquired, ToTransition(StrippedTransition),
                                 ToRequired(StrippedTransition)};
              const auto ContainsAcquired =
                  [&Acquired](const auto &IndexedVertex) {
                    return Value(IndexedVertex).contains(Acquired);
                  };
              const auto AllAreIndependentOfTransition =
                  [&Transition](const auto &IndependentTransitions) {
                    return ranges::all_of(IndependentTransitions,
                                          independentOf(Transition));
                  };
              ranges::for_each(
                  ranges::views::zip(InterestingVertices,
                                     CurrentIndependentTransitionsVec) |
                      ranges::views::filter(ContainsAcquired, Element<0>) |
                      ranges::views::transform(Element<1>) |
                      ranges::views::filter(AllAreIndependentOfTransition),
                  ranges::push_back(Transition));
            };
        ranges::for_each(BundeledTransition.second,
                         MaybeAddIndependentTransitions);
        return CurrentIndependentTransitionsVec;
      });

  return ranges::views::zip(InterestingVertices | ranges::views::move,
                            IndependentTransitionsVec | ranges::views::move) |
         ranges::to_vector;
}

[[nodiscard]] auto toTransitionAndTargetTypeSetPairForVertex(
    const indexed_value<GraphBuilder::VertexType> &IndexedVertex) {
  return [&IndexedVertex](const TransitionType &Transition) {
    return std::pair{Transition,
                     Value(IndexedVertex) |
                         ranges::views::remove(ToAcquired(Transition)) |
                         ranges::views::set_union(ToRequired(Transition)) |
                         ranges::to<TypeSet>};
  };
}
} // namespace

void GraphBuilder::build() {
  while (CurrentState_.IterationIndex < Query_.getConfig().MaxGraphDepth &&
         buildStep()) {
    // complete build
  }
}

bool GraphBuilder::buildStep() {
  return buildStepFor(CurrentState_.InterestingVertices);
}

bool GraphBuilder::buildStepFor(const VertexDescriptor Vertex) {
  return buildStepFor(VertexData_ |
                      ranges::views::filter(EqualTo(Vertex), Index) |
                      ranges::to<VertexSet>);
}

bool GraphBuilder::buildStepFor(const VertexType &InterestingVertex) {
  return buildStepFor(VertexData_ |
                      ranges::views::filter(EqualTo(InterestingVertex), Value) |
                      ranges::to<VertexSet>);
}

bool GraphBuilder::buildStepFor(VertexSet InterestingVertices) {
  CurrentState_.InterestingVertices.clear();
  ++CurrentState_.IterationIndex;
  return ranges::fold_left(
      constructVertexAndTransitionsPairVector(std::move(InterestingVertices),
                                              TransitionsForQuery_),
      false, [this](bool AddedTransitions, const auto &VertexAndTransitions) {
        const auto &[IndexedVertex, Transitions] = VertexAndTransitions;
        return ranges::fold_left(
            Transitions |
                ranges::views::transform(
                    toTransitionAndTargetTypeSetPairForVertex(IndexedVertex)),
            AddedTransitions, maybeAddEdgeFrom(IndexedVertex));
      });
}

std::pair<GraphType, GraphData> GraphBuilder::commit() {
  GraphData Data{};
  Data.VertexData = getIndexedSetSortedByIndex(std::move(VertexData_));
  Data.Edges = getIndexedSetSortedByIndex(std::move(EdgesData_));
  Data.EdgeIndices =
      ranges::views::indices(Data.Edges.size()) | ranges::to_vector;
  Data.EdgeWeights = std::move(EdgeWeights_);
  return {GraphType(Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
                    Data.EdgeIndices.data(), Data.EdgeIndices.size()),
          std::move(Data)};
}

std::pair<GraphType, GraphData> createGraph(const QueryType &Query) {
  auto Builder = GraphBuilder{Query};
  Builder.build();
  return Builder.commit();
}

VertexDescriptor
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const TypeSet &QueriedType) {
  const auto SourceVertex = ranges::find(Data.VertexData, QueriedType);

  if (SourceVertex == Data.VertexData.end()) {
    throw GetMeException(fmt::format("found no type matching {}", QueriedType));
  }
  return static_cast<VertexDescriptor>(
      std::distance(Data.VertexData.begin(), SourceVertex));
}

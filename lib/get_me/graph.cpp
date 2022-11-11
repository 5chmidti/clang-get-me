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
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/remove.hpp>
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
                return EdgeWeights[Index(IndexedEdge)];
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
              return Value(IndexedVertex).contains(Acquired);
            };
        const auto AllAreIndependentOfTransition =
            [&Transition](
                const std::vector<TransitionType> &IndependentTransitions) {
              return ranges::all_of(IndependentTransitions,
                                    independentOf(Transition));
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

[[nodiscard]] static auto toTransitionAndTargetTypeSetPairForVertex(
    const indexed_value_type<TypeSet> &IndexedVertex) {
  return [&IndexedVertex](const TransitionType &Transition) {
    return std::pair{Transition,
                     Value(IndexedVertex) |
                         ranges::views::remove(acquired(Transition)) |
                         ranges::views::set_union(required(Transition)) |
                         ranges::to<TypeSet>};
  };
}

void GraphBuilder::build() {
  while (CurrentState_.IterationIndex < Query_.getConfig().MaxGraphDepth &&
         buildStep()) {
    // complete build
  }
}

bool GraphBuilder::buildStep() {
  return buildStepFor(CurrentState_.InterestingVertices);
}

bool GraphBuilder::buildStepFor(VertexDescriptor Vertex) {
  return buildStepFor(
      VertexData_ |
      ranges::views::filter(
          [Vertex](const size_t VertexIndex) { return VertexIndex == Vertex; },
          Index) |
      ranges::to<indexed_set<TypeSet>>);
}

bool GraphBuilder::buildStepFor(const TypeSet &InterestingVertex) {
  return buildStepFor(VertexData_ |
                      ranges::views::filter(
                          [&InterestingVertex](const auto &Vertex) {
                            return Vertex == InterestingVertex;
                          },
                          Value) |
                      ranges::to<indexed_set<TypeSet>>);
}

bool GraphBuilder::buildStepFor(
    const indexed_set<TypeSet> &InterestingVertices) {
  auto AddedTransitions = false;
  StepState NewState{.IterationIndex = CurrentState_.IterationIndex + 1};
  for (auto [IndexedVertex, Transitions] :
       constructVertexAndTransitionsPairVector(InterestingVertices,
                                               TransitionsForQuery_)) {
    for (const auto &[Transition, TargetTypeSet] :
         Transitions |
             ranges::views::transform(
                 toTransitionAndTargetTypeSetPairForVertex(IndexedVertex))) {
      const auto TargetVertexIter = VertexData_.find(TargetTypeSet);
      const auto TargetVertexExists = TargetVertexIter != VertexData_.end();
      const auto TargetVertexIndex =
          TargetVertexExists ? Index(*TargetVertexIter) : VertexData_.size();

      const auto EdgeToAdd =
          GraphData::EdgeType{Index(IndexedVertex), TargetVertexIndex};

      if (TargetVertexExists &&
          edgeWithTransitionExistsInContainer(EdgesData_, EdgeToAdd, Transition,
                                              EdgeWeights_)) {
        continue;
      }

      if (TargetVertexExists) {
        NewState.InterestingVertices.emplace(*TargetVertexIter);
      } else {
        NewState.InterestingVertices.emplace(TargetVertexIndex, TargetTypeSet);
        VertexData_.emplace(TargetVertexIndex, TargetTypeSet);
      }
      if (const auto [_, EdgeAdded] =
              EdgesData_.emplace(EdgesData_.size(), EdgeToAdd);
          EdgeAdded) {
        EdgeWeights_.push_back(Transition);
        AddedTransitions = true;
      }
    }
  }
  CurrentState_ = std::move(NewState);
  return AddedTransitions;
}

std::pair<GraphType, GraphData> GraphBuilder::commit() {
  GraphData Data{};
  Data.VertexData = getIndexedSetSortedByIndex(std::move(VertexData_));
  Data.Edges = getIndexedSetSortedByIndex(std::move(EdgesData_));
  Data.EdgeIndices =
      ranges::views::iota(static_cast<size_t>(0U), Data.Edges.size()) |
      ranges::to_vector;
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

std::optional<VertexDescriptor>
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const TypeSet &QueriedType) {
  const auto SourceVertex = ranges::find(Data.VertexData, QueriedType);

  if (SourceVertex == Data.VertexData.end()) {
    spdlog::error("found no type matching {} in {}", QueriedType,
                  Data.VertexData);
    return std::nullopt;
  }
  return static_cast<VertexDescriptor>(
      std::distance(Data.VertexData.begin(), SourceVertex));
}

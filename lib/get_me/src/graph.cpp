#include "get_me/graph.hpp"

#include <iterator>
#include <set>
#include <utility>
#include <vector>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <range/v3/action/push_back.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/fold_left.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/subrange.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include "get_me/config.hpp"
#include "get_me/indexed_set.hpp"
#include "get_me/query.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/functional.hpp"
#include "support/ranges/projections.hpp"

bool edgeWithTransitionExistsInContainer(
    const indexed_set<GraphData::EdgeType> &Edges,
    const GraphData::EdgeType &EdgeToAdd, const TransitionType &Transition,
    const std::vector<TransitionType> &EdgeTransitions) {
  return ranges::contains(
      ranges::subrange(Edges.lower_bound(EdgeToAdd),
                       Edges.upper_bound(EdgeToAdd)) |
          ranges::views::transform(Lookup(EdgeTransitions, Index)),
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

GraphData::GraphData(std::vector<VertexDataType> VertexData,
                     std::vector<size_t> VertexDepth,
                     std::vector<EdgeType> Edges,
                     std::vector<TransitionType> EdgeTransitions)
    : VertexData{std::move(VertexData)},
      VertexDepth{std::move(VertexDepth)},
      Edges{std::move(Edges)},
      EdgeIndices{ranges::views::indices(this->Edges.size()) |
                  ranges::to_vector},
      EdgeTransitions{std::move(EdgeTransitions)},
      Graph{this->Edges.data(), this->Edges.data() + this->Edges.size(),
            this->EdgeIndices.data(), this->EdgeIndices.size()} {}

void GraphBuilder::build() {
  while (CurrentState_.IterationIndex < Conf_.MaxGraphDepth && buildStep()) {
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

GraphData GraphBuilder::commit() {
  return {getIndexedSetSortedByIndex(std::move(VertexData_)),
          getIndexedSetSortedByIndex(std::move(VertexDepth_)),
          getIndexedSetSortedByIndex(std::move(EdgesData_)),
          std::move(EdgeTransitions_)};
}

GraphData createGraph(const TransitionCollector &Transitions,
                      const TypeSetValueType &Query, const Config &Conf) {
  auto Builder = GraphBuilder{Transitions, Query, Conf};
  Builder.build();
  return Builder.commit();
}

VertexDescriptor
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const TypeSetValueType &QueriedType) {
  const auto SourceVertex = ranges::find(Data.VertexData, TypeSet{QueriedType});

  if (SourceVertex == Data.VertexData.end()) {
    throw GetMeException(fmt::format("found no type matching {}", QueriedType));
  }
  return static_cast<VertexDescriptor>(
      std::distance(Data.VertexData.begin(), SourceVertex));
}

size_t GraphBuilder::getVertexDepth(const size_t VertexIndex) const {
  return VertexDepth_ | ranges::views::filter(EqualTo(VertexIndex), Index) |
         ranges::views::take(1) | ranges::views::values | ranges::to_vector |
         actions::FrontOr(0U);
}

std::string fmt::formatter<GraphData>::toDotFormat(const GraphData &Data) {
  const auto IndexMap = boost::get(boost::edge_index, Data.Graph);

  const auto ToString = [&Data, &IndexMap](const auto &Edge) {
    const auto SourceNode = Source(Edge);
    const auto TargetNode = Target(Edge);

    const auto Transition =
        ToTransition(Data.EdgeTransitions[boost::get(IndexMap, Edge)]);
    const auto TargetVertex = Data.VertexData[TargetNode];
    const auto SourceVertex = Data.VertexData[SourceNode];

    auto EdgeWeightAsString = fmt::format("{}", Transition);
    boost::replace_all(EdgeWeightAsString, "\"", "\\\"");
    return fmt::format(
        R"(  "{}" -> "{}"[label="{}"]
)",
        SourceVertex, TargetVertex, EdgeWeightAsString);
  };

  return ranges::fold_left(toRange(boost::edges(Data.Graph)) |
                               ranges::views::transform(ToString),
                           std::string{"digraph D {\n  layout = \"sfdp\";\n"},
                           [](std::string Result, auto Line) {
                             Result.append(std::move(Line));
                             return Result;
                           }) +
         "}\n";
}

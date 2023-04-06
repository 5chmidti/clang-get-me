#include "get_me/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string/replace.hpp>
#include <fmt/core.h>
#include <range/v3/action/push_back.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/action/unique.hpp>
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
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/front.hpp"
#include "support/ranges/functional.hpp"

namespace {
[[nodiscard]] bool
edgeWithTransitionExistsInContainer(const GraphData::EdgeContainer &Edges,
                                    const TransitionEdgeType &EdgeToAdd,
                                    const TransitionType &Transition) {
  return ranges::contains(
      ranges::subrange(Edges.lower_bound(EdgeToAdd),
                       Edges.upper_bound(EdgeToAdd)) |
          ranges::views::transform(&TransitionEdgeType::TransitionIndex),
      Index(Transition));
}

[[nodiscard]] std::vector<
    std::pair<GraphBuilder::VertexSet::value_type, std::vector<TransitionType>>>
constructVertexAndTransitionsPairVector(
    GraphBuilder::VertexSet InterestingVertices,
    const TransitionCollector::associative_container_type &Transitions) {
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
                  TransitionType{Index(StrippedTransition),
                                 {Acquired, ToTransition(StrippedTransition),
                                  ToRequired(StrippedTransition)}};
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

[[nodiscard]] std::vector<VertexDescriptor>
getVerticesThatAreNotA(GraphData &Data, const auto SourceOrTargetProjection) {
  const auto Vertices =
      Data.Edges | ranges::views::transform(SourceOrTargetProjection) |
      ranges::to_vector | ranges::actions::sort | ranges::actions::unique;
  return ranges::views::set_difference(
             ranges::views::indices(Data.VertexData.size()), Vertices) |
         ranges::to_vector;
}
} // namespace

GraphData::GraphData(std::vector<TypeSet> VertexData,
                     std::vector<size_t> VertexDepth, EdgeContainer Edges,
                     std::shared_ptr<TransitionCollector> Transitions,
                     PathContainer Paths)
    : VertexData{std::move(VertexData)},
      VertexDepth{std::move(VertexDepth)},
      Edges{std::move(Edges)},
      Paths{std::move(Paths)},
      Transitions{std::move(Transitions)} {}

GraphData::GraphData(std::vector<TypeSet> VertexData,
                     std::vector<size_t> VertexDepth, EdgeContainer Edges,
                     std::shared_ptr<TransitionCollector> Transitions)
    : GraphData{std::move(VertexData),
                std::move(VertexDepth),
                std::move(Edges),
                std::move(Transitions),
                {}} {}

void GraphBuilder::build() {
  while (CurrentState_.IterationIndex < Conf_->MaxGraphDepth && buildStep()) {
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

  auto MaybeAddEdgeFrom =
      [this](const indexed_value<VertexType> &IndexedSourceVertex) {
        const auto SourceDepth = getVertexDepth(Index(IndexedSourceVertex));
        return [this, &IndexedSourceVertex,
                SourceDepth](bool AddedTransitions,
                             const std::pair<TransitionType, TypeSet>
                                 &TransitionAndTargetTS) {
          const auto &[Transition, TargetTypeSet] = TransitionAndTargetTS;
          const auto TargetVertexIter = VertexData_.find(TargetTypeSet);
          const auto TargetVertexExists = TargetVertexIter != VertexData_.end();
          const auto TargetVertexIndex = TargetVertexExists
                                             ? Index(*TargetVertexIter)
                                             : VertexData_.size();

          const auto EdgeToAdd = TransitionEdgeType{
              {Index(IndexedSourceVertex), TargetVertexIndex},
              Index(Transition)};

          const auto IsEmptyTargetTS = TargetVertexIndex == 1U;
          if (TargetVertexExists) {
            if (!IsEmptyTargetTS && !Conf_->EnableGraphBackwardsEdge &&
                SourceDepth >= getVertexDepth(TargetVertexIndex)) {
              return AddedTransitions;
            }

            if (edgeWithTransitionExistsInContainer(Edges_, EdgeToAdd,
                                                    Transition)) {
              return AddedTransitions;
            }
          }
          CurrentState_.InterestingVertices.emplace(TargetVertexIndex,
                                                    TargetTypeSet);
          VertexData_.emplace(TargetVertexIndex, TargetTypeSet);
          VertexDepth_.emplace(TargetVertexIndex, CurrentState_.IterationIndex);
          if (const auto [_, EdgeAdded] = Edges_.emplace(EdgeToAdd);
              EdgeAdded) {
            AddedTransitions = true;
          }

          return AddedTransitions;
        };
      };

  return ranges::fold_left(
      constructVertexAndTransitionsPairVector(
          std::move(InterestingVertices),
          getTransitionsForQuery(Transitions_->Data, Query_)),
      false,
      [MaybeAddEdgeFrom](bool AddedTransitions,
                         const auto &VertexAndTransitions) {
        const auto &[IndexedVertex, Transitions] = VertexAndTransitions;
        return ranges::fold_left(
            Transitions |
                ranges::views::transform(
                    toTransitionAndTargetTypeSetPairForVertex(IndexedVertex)),
            AddedTransitions, MaybeAddEdgeFrom(IndexedVertex));
      });
}

GraphData GraphBuilder::commit() {
  return {getIndexedSetSortedByIndex(std::move(VertexData_)),
          getIndexedSetSortedByIndex(std::move(VertexDepth_)), Edges_,
          Transitions_, std::move(Paths_)};
}

GraphData
runGraphBuilding(const std::shared_ptr<TransitionCollector> &Transitions,
                 const TypeSetValueType &Query, std::shared_ptr<Config> Conf) {
  auto Builder = GraphBuilder{Transitions, Query, std::move(Conf)};
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
  const auto ToString = [&Data](const TransitionEdgeType &Edge) {
    const auto Transition =
        ToTransition(Data.Transitions->FlatData[Edge.TransitionIndex]);
    const auto TargetVertex = Data.VertexData[Target(Edge)];
    const auto SourceVertex = Data.VertexData[Source(Edge)];

    auto EdgeWeightAsString = fmt::format("{}", Transition);
    boost::replace_all(EdgeWeightAsString, "\"", "\\\"");
    return fmt::format(
        R"(  "{}" -> "{}"[label="{}"]
)",
        SourceVertex, TargetVertex, EdgeWeightAsString);
  };

  return ranges::fold_left(Data.Edges | ranges::views::transform(ToString),
                           std::string{"digraph D {\n  layout = \"sfdp\";\n"},
                           [](std::string Result, auto Line) {
                             Result.append(std::move(Line));
                             return Result;
                           }) +
         "}\n";
}

std::int64_t GraphBuilder::getVertexDepthDifference(const size_t SourceDepth,
                                                    const size_t TargetDepth) {
  GetMeException::verify(TargetDepth <=
                             std::numeric_limits<std::int64_t>::max(),
                         "TargetDepth is to large for cast to int64_t");
  GetMeException::verify(SourceDepth <=
                             std::numeric_limits<std::int64_t>::max(),
                         "SourceDepth is to large for cast to int64_t");

  return static_cast<std::int64_t>(TargetDepth) -
         static_cast<std::int64_t>(SourceDepth);
}

std::vector<VertexDescriptor> getRootVertices(GraphData &Data) {
  return getVerticesThatAreNotA(Data, Target);
}

std::vector<VertexDescriptor> getLeafVertices(GraphData &Data) {
  return getVerticesThatAreNotA(Data, Source);
}

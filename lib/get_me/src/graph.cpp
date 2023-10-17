#include "get_me/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string/replace.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <range/v3/action/push_back.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/action/unique.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/fold_left.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/functional/bind_back.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/drop_while.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/repeat.hpp>
#include <range/v3/view/repeat_n.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/single.hpp>
#include <range/v3/view/subrange.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/take_while.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/indexed_set.hpp"
#include "get_me/query.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_conversion_map.hpp"
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

using FoldType = std::vector<std::pair<GraphBuilder::VertexSet::value_type,
                                       std::vector<TransitionType>>>;
[[nodiscard]] FoldType constructVertexAndTransitionsPairVector(
    GraphBuilder::VertexSet InterestingVertices,
    const TransitionData::associative_container_type &Transitions,
    const TypeConversionMap &ConversionMap) {
  return ranges::fold_left(
      Transitions,
      ranges::views::zip(InterestingVertices,
                         ranges::views::repeat(std::vector<TransitionType>{})) |
          ranges::to<FoldType>,
      [&ConversionMap](const FoldType &VertexAndTransitionsPair,
                       const BundeledTransitionType &BundeledTransition) {
        const auto Acquired = ToAcquired(BundeledTransition);
        const auto PossibleConversionsTypesForAcquired =
            ConversionMap |
            ranges::views::filter(ranges::bind_back(ranges::contains, Acquired),
                                  Value) |
            ranges::views::keys | ranges::to<TypeSet>;
        spdlog::error("VertexAndTransitionsPair: {}", VertexAndTransitionsPair);
        return VertexAndTransitionsPair |
               ranges::views::transform(
                   [&PossibleConversionsTypesForAcquired,
                    &BundeledTransition](const auto &Pair) {
                     const auto &[Vertex, TransitionsVec] = Pair;
                     const auto GetNewTransitions =
                         [&PossibleConversionsTypesForAcquired](
                             auto &InterestingVertex) {
                           return [&InterestingVertex,
                                   &PossibleConversionsTypesForAcquired](
                                      const auto &StrippedTransition) {
                             const auto MakeTransition =
                                 [&StrippedTransition](
                                     const auto &ConversionTypeOfAcquired) {
                                   return TransitionType{
                                       Index(StrippedTransition),
                                       {ConversionTypeOfAcquired,
                                        ToTransition(StrippedTransition),
                                        ToRequired(StrippedTransition)}};
                                 };
                             const auto MatchingTypes =
                                 ranges::views::set_intersection(
                                     Value(InterestingVertex),
                                     PossibleConversionsTypesForAcquired);
                             return MatchingTypes |
                                    ranges::views::transform(MakeTransition);
                           };
                         };
                     spdlog::warn("Vertex: {}", Vertex);
                     return std::pair{
                         Vertex, getSmallestIndependentTransitions(
                                     ranges::views::concat(
                                         TransitionsVec,
                                         BundeledTransition.second |
                                             ranges::views::for_each(
                                                 GetNewTransitions(Vertex))) |
                                     ranges::to<std::vector<TransitionType>>)};
                   }) |
               ranges::to<FoldType>;
      });
}

// FIXME: this should be a positive match? would probably be cheaper to compute
[[nodiscard]] std::vector<VertexDescriptor>
getVerticesThatAreNotA(const GraphData &Data,
                       const auto SourceOrTargetProjection) {
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
                     std::shared_ptr<TransitionData> Transitions,
                     std::shared_ptr<Config> Conf)
    : VertexData{std::move(VertexData)},
      VertexDepth{std::move(VertexDepth)},
      Edges{std::move(Edges)},
      Transitions{std::move(Transitions)},
      Conf{std::move(Conf)} {}

class GraphBuilder::GraphBuilderImpl {
public:
  [[nodiscard]] auto toTransitionAndTargetTypeSetPairForVertex(
      const indexed_value<GraphBuilder::VertexType> &IndexedVertex) {
    return [&IndexedVertex, this](const TransitionType &Transition) {
      const auto Acquired = ToAcquired(Transition);
      const auto ConversionsOfAcquired =
          Transitions->ConversionMap.find(Acquired);

      GetMeException::verify(
          ConversionsOfAcquired != Transitions->ConversionMap.end(),
          "Could not find type conversion mapping for {}", Acquired);

      const auto NewRequired =
          Value(IndexedVertex) |
          ranges::views::set_difference(ConversionsOfAcquired->second) |
          ranges::views::set_union(ToRequired(Transition)) |
          ranges::to<TypeSet>;
      spdlog::error(
          "toTransitionAndTargetTypeSetPairForVertex: {} with {} and {}",
          IndexedVertex, Transition, NewRequired);
      return std::pair{Transition, NewRequired};
    };
  }

  TransitionData *Transitions;
};

GraphBuilder::GraphBuilder(std::shared_ptr<TransitionData> Transitions,
                           TypeSet Query, std::shared_ptr<Config> Conf)
    : Transitions_{std::move(Transitions)},
      Query_{std::move(Query)},
      VertexData_{ranges::views::concat(
                      ranges::views::enumerate(Query_) |
                          ranges::views::transform(
                              [](const auto Pair) -> VertexSet::value_type {
                                return {std::get<0>(Pair), {std::get<1>(Pair)}};
                              }),
                      ranges::views::single(VertexSet::value_type{
                          ranges::size(Query_), TypeSet{}})) |
                  ranges::to<VertexSet>},
      VertexDepth_{
          ranges::views::concat(
              ranges::views::repeat_n(
                  size_t{0U}, static_cast<std::int64_t>(ranges::size(Query_))),
              ranges::views::single(size_t{1U})) |
          ranges::to_vector},
      Conf_{std::move(Conf)},
      CurrentState_{0U, VertexData_},
      Impl_{std::make_unique<GraphBuilderImpl>(Transitions_.get())} {}

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
  return buildStepFor(
      ranges::subrange{VertexData_.lower_bound(InterestingVertex),
                       VertexData_.upper_bound(InterestingVertex)} |
      ranges::to<VertexSet>);
}

bool GraphBuilder::buildStepFor(VertexSet InterestingVertices) {
  CurrentState_.InterestingVertices.clear();
  ++CurrentState_.IterationIndex;
  spdlog::error("");
  spdlog::error("InterestingVertices: {}", InterestingVertices);

  auto MaybeAddEdgeFrom =
      [this](const indexed_value<VertexType> &IndexedSourceVertex) {
        const auto SourceDepth = VertexDepth_[Index(IndexedSourceVertex)];
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

          spdlog::error("TargetTypeSet: {}", TargetTypeSet);
          spdlog::error("TargetVertexExists: {}", TargetVertexExists);
          spdlog::error("VertexData_: {}", VertexData_);
          spdlog::error("TargetVertexIndex: {}", TargetVertexIndex);
          spdlog::error("IndexedSourceVertex: {}", IndexedSourceVertex);
          spdlog::error("TransitionAndTargetTS: {}", TransitionAndTargetTS);

          const auto EdgeToAdd = TransitionEdgeType{
              {Index(IndexedSourceVertex), TargetVertexIndex},
              Index(Transition)};

          spdlog::error("EdgeToAdd: {}", EdgeToAdd);

          const auto IsEmptyTargetTS = TargetVertexIndex == 1U;
          if (TargetVertexExists) {
            if (!IsEmptyTargetTS && !Conf_->EnableGraphBackwardsEdge &&
                SourceDepth >= VertexDepth_[TargetVertexIndex]) {
              spdlog::error("backwards edge filter");
              return AddedTransitions;
            }

            if (edgeWithTransitionExistsInContainer(Edges_, EdgeToAdd,
                                                    Transition)) {
              spdlog::error("edge exists");
              return AddedTransitions;
            }
          }
          CurrentState_.InterestingVertices.emplace(TargetVertexIndex,
                                                    TargetTypeSet);
          if (!TargetVertexExists) {
            spdlog::error("adding vertex: {} -> {}", TargetVertexIndex,
                          TargetTypeSet);
            VertexData_.emplace(TargetVertexIndex, TargetTypeSet);
            VertexDepth_.push_back(CurrentState_.IterationIndex);
          }
          if (const auto [_, EdgeAdded] = Edges_.emplace(EdgeToAdd);
              EdgeAdded) {
            spdlog::error("added edge: {}", EdgeToAdd);
            AddedTransitions = true;
          }

          return AddedTransitions;
        };
      };

  spdlog::error("Transitions_->Data: {}", Transitions_->Data);
  auto TransitionsForQuery = getTransitionsForQuery(Transitions_->Data, Query_);
  // FIXME: getTransitionsForQuery seems broken
  auto VertexAndTransitionsVec = constructVertexAndTransitionsPairVector(
      std::move(InterestingVertices), TransitionsForQuery,
      Transitions_->ConversionMap);
  spdlog::error("VertexAndTransitionsVec: {}", VertexAndTransitionsVec);
  spdlog::error("TransitionsForQuery: {}", TransitionsForQuery);
  return ranges::fold_left(
      VertexAndTransitionsVec, false,
      [this, MaybeAddEdgeFrom](bool AddedTransitions,
                               const auto &VertexAndTransitions) {
        const auto &[IndexedVertex, Transitions] = VertexAndTransitions;

        spdlog::error("VertexAndTransitions: {}", VertexAndTransitions);

        const auto MaxAllowedTypeSetSize =
            SafePlus(Conf_->MaxPathLength, Conf_->MaxRemainingTypes);
        // adjust with -1 to correctly model removing the acquired in
        // 'new = old-acquired+required'
        const auto CurrentTypeSetSize =
            SafePlus(ranges::size(Value(IndexedVertex)),
                     CurrentState_.IterationIndex) -
            1;

        return ranges::fold_left(
            Transitions |
                ranges::views::filter(
                    Less(MaxAllowedTypeSetSize),
                    ranges::compose(
                        ranges::bind_back(SafePlus, CurrentTypeSetSize),
                        ranges::compose(ranges::size, ToRequired))) |
                ranges::views::transform(
                    Impl_->toTransitionAndTargetTypeSetPairForVertex(
                        IndexedVertex)),
            AddedTransitions, MaybeAddEdgeFrom(IndexedVertex));
      });
}

GraphData GraphBuilder::commit() {
  return {getIndexedSetSortedByIndex(std::move(VertexData_)),
          std::move(VertexDepth_), Edges_, Transitions_, std::move(Conf_)};
}

GraphData runGraphBuilding(const std::shared_ptr<TransitionData> &Transitions,
                           const TypeSet &Query, std::shared_ptr<Config> Conf) {
  auto Builder = GraphBuilder{Transitions, Query, std::move(Conf)};
  Builder.build();
  return Builder.commit();
}

std::string fmt::formatter<GraphData>::toDotFormat(const GraphData &Data) {
  const auto ToString = [&Data](const TransitionEdgeType &Edge) {
    const auto Transition =
        ToTransition(Data.Transitions->FlatData[Edge.TransitionIndex]);
    const auto TargetVertex = Target(Edge);
    const auto SourceVertex = Source(Edge);

    auto EdgeWeightAsString = fmt::format("{}", Transition);
    boost::replace_all(EdgeWeightAsString, "\"", "\\\"");
    return fmt::format(R"(  {} -> {}[label="{}"])", SourceVertex, TargetVertex,
                       EdgeWeightAsString);
  };

  const auto VertexToString = [](const auto &IndexedVertex) {
    return fmt::format("  {} [label=\"{}\"]", Index(IndexedVertex),
                       Value(IndexedVertex));
  };
  const auto ToRootVertexModifier = [](const auto &IndexedVertex) {
    return fmt::format("  {} [color=red]", Index(IndexedVertex),
                       Value(IndexedVertex));
  };
  const auto ToLeafVertexModifier = [](const auto &IndexedVertex) {
    return fmt::format("  {} [color=orange]", Index(IndexedVertex),
                       Value(IndexedVertex));
  };

  const auto RootVertices =
      Data.VertexData | ranges::views::enumerate |
      ranges::views::take_while(ranges::not_fn(ranges::empty), Value);

  const auto LeafVertices =
      Data.VertexData | ranges::views::enumerate |
      ranges::views::set_difference(
          Data.Edges | ranges::views::transform(Source), std::less{}, Index);

  const auto Vertices = fmt::format(
      "{}\n\n{}\n\n{}",
      fmt::join(Data.VertexData | ranges::views::enumerate |
                    ranges::views::transform(VertexToString),
                "\n"),
      fmt::join(RootVertices | ranges::views::transform(ToRootVertexModifier),
                "\n"),
      fmt::join(LeafVertices | ranges::views::transform(ToLeafVertexModifier),
                "\n"));

  return fmt::format(
      "digraph D {{\n  layout = \"sfdp\";\n{}\n\n{}\n}}\n", Vertices,
      fmt::join(Data.Edges | ranges::views::transform(ToString), "\n"));
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

std::vector<VertexDescriptor> getRootVertices(const GraphData &Data) {
  return getVerticesThatAreNotA(Data, Target);
}

std::vector<VertexDescriptor> getLeafVertices(const GraphData &Data) {
  return getVerticesThatAreNotA(Data, Source);
}

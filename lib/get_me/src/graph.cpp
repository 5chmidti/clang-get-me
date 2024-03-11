#include "get_me/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string/replace.hpp>
#include <boost/container/flat_set.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <range/v3/action/push_back.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/action/unique.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/fold_left.hpp>
#include <range/v3/functional/bind_back.hpp>
#include <range/v3/functional/comparisons.hpp>
#include <range/v3/functional/compose.hpp>
#include <range/v3/functional/not_fn.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/repeat.hpp>
#include <range/v3/view/repeat_n.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/single.hpp>
#include <range/v3/view/subrange.hpp>
#include <range/v3/view/take_while.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

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
      ToBundeledTransitionIndex(Transition));
}

[[nodiscard]] TransitionType
replaceAcquiredTypeOfTransition(const TransparentType &ConversionTypeOfAcquired,
                                TransitionType Transition) {
  ToAcquired(Transition) = ConversionTypeOfAcquired;
  return Transition;
}

[[nodiscard]] auto generateTransitionsFromConversionTypes(
    const boost::container::flat_set<Type> &PossibleConversionsTypesForAcquired,
    const GraphBuilder::VertexType &InterestingVertex,
    const TransitionType &Transition) {
  const auto MatchingTypes = ranges::views::set_intersection(
      InterestingVertex, PossibleConversionsTypesForAcquired, ranges::less{},
      &TransparentType::Desugared);
  return MatchingTypes | ranges::views::transform(ranges::bind_back(
                             replaceAcquiredTypeOfTransition, Transition));
}

using FoldType =
    std::vector<std::pair<GraphBuilder::VertexSet::value_type,
                          boost::container::flat_set<TransitionType>>>;
[[nodiscard]] FoldType constructVertexAndTransitionsPairVector(
    GraphBuilder::VertexSet InterestingVertices,
    const TransitionMap &Transitions, const TypeConversionMap &ConversionMap) {
  return ranges::fold_left(
      Transitions,
      ranges::views::zip(
          InterestingVertices,
          ranges::views::repeat(boost::container::flat_set<TransitionType>{})) |
          ranges::to<FoldType>,
      [&ConversionMap](const FoldType &VertexAndTransitionsPairs,
                       const TransitionType &Transition) {
        const auto PossibleConversionsTypesForAcquired =
            ConversionMap |
            ranges::views::filter(
                ranges::bind_back(ranges::contains, ToAcquired(Transition)),
                Value) |
            ranges::views::keys | ranges::to<boost::container::flat_set<Type>>;
        return VertexAndTransitionsPairs |
               ranges::views::transform([&PossibleConversionsTypesForAcquired,
                                         &Transition](const auto &Pair) {
                 const auto &[Vertex, TransitionsVec] = Pair;
                 return std::pair{
                     Vertex, getSmallestIndependentTransitions(
                                 ranges::views::concat(
                                     TransitionsVec,
                                     generateTransitionsFromConversionTypes(
                                         PossibleConversionsTypesForAcquired,
                                         Value(Vertex), Transition)) |
                                 ranges::to_vector)};
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

std::vector<std::vector<FlatTransitionType>>
expandAndFlattenPath(const PathType &Path, const GraphData &Data) {
  const auto ExpandEdge =
      [&Data](const TransitionEdgeType &Edge) -> decltype(auto) {
    const auto &Transition =
        Data.Transitions->BundeledData[Edge.TransitionIndex];
    return ranges::views::zip(ranges::views::repeat(ToAcquired(Transition)),
                              ToTransitions(Transition) | ranges::views::values,
                              ranges::views::repeat(ToRequired(Transition)));
  };
  auto ExpandedPath =
      Path | ranges::views::transform(ExpandEdge) | ranges::to_vector;
  if (ranges::empty(ExpandedPath)) {
    return {};
  }
  return ranges::fold_left(
      ExpandedPath | ranges::views::drop(1),
      ranges::front(ExpandedPath) |
          ranges::views::transform(
              [](const auto &Val) { return std::vector{Val}; }) |
          ranges::to<std::vector<std::vector<FlatTransitionType>>>,
      [](const std::vector<std::vector<FlatTransitionType>> &FoldRange,
         const auto &ExpandedPathStep) {
        return ranges::views::cartesian_product(FoldRange, ExpandedPathStep) |
               ranges::views::transform([](auto Pair) {
                 return Copy(Element<0>(Pair)) |
                        ranges::actions::push_back(Element<1>(Pair));
               }) |
               ranges::to_vector;
      });
}

class GraphBuilder::GraphBuilderImpl {
public:
  [[nodiscard]] auto toTransitionAndTargetTypeSetPairForVertex(
      const indexed_value<GraphBuilder::VertexType> &IndexedVertex) {
    return [&IndexedVertex, this](const TransitionType &Transition) {
      const auto Acquired = ToAcquired(Transition);
      const auto ConversionsOfAcquired =
          Transitions->ConversionMap.find(Acquired.Desugared);

      GetMeException::verify(
          ConversionsOfAcquired != Transitions->ConversionMap.end(),
          "Could not find type conversion mapping for {}", Acquired);

      const auto NewRequired =
          Value(IndexedVertex) |
          ranges::views::set_difference(ConversionsOfAcquired->second) |
          ranges::views::set_union(ToRequired(Transition)) |
          ranges::to<TypeSet>;
      return std::pair{Transition, NewRequired};
    };
  }

  TransitionData *Transitions;
};

GraphBuilder::GraphBuilder(std::shared_ptr<TransitionData> Transitions,
                           TypeSet Query, std::shared_ptr<Config> Conf)
    : Transitions_{std::move(Transitions)},
      Query_{std::move(Query)},
      EmptyTsIndex_{ranges::size(Query_)},
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

  const auto NumGraphBuildingStepsLeft =
      Conf_->MaxGraphDepth - this->CurrentState_.IterationIndex;
  const auto MaxAllowedSizeOfTargetVertex =
      (SafePlus(Conf_->MaxLeafVertexSize, NumGraphBuildingStepsLeft));

  auto MaybeAddEdgeFrom = [this, MaxAllowedSizeOfTargetVertex](
                              const indexed_value<VertexType>
                                  &IndexedSourceVertex) {
    const auto SourceDepth = VertexDepth_[Index(IndexedSourceVertex)];
    return
        [this, &IndexedSourceVertex, SourceDepth, MaxAllowedSizeOfTargetVertex](
            bool AddedTransitions,
            const std::pair<TransitionType, TypeSet> &TransitionAndTargetTS) {
          const auto &[Transition, TargetTypeSet] = TransitionAndTargetTS;
          const auto TargetVertexIter = VertexData_.find(TargetTypeSet);
          const auto TargetVertexExists = TargetVertexIter != VertexData_.end();
          const auto TargetVertexIndex = TargetVertexExists
                                             ? Index(*TargetVertexIter)
                                             : VertexData_.size();

          const auto EdgeToAdd = TransitionEdgeType{
              {Index(IndexedSourceVertex), TargetVertexIndex},
              ToBundeledTransitionIndex(Transition)};
          if (ranges::size(TargetTypeSet) > MaxAllowedSizeOfTargetVertex) {
            return AddedTransitions;
          }

          if (TargetVertexExists) {
            if (!isEmptyTargetTS(TargetVertexIndex) &&
                !Conf_->EnableGraphBackwardsEdge &&
                SourceDepth >= VertexDepth_[TargetVertexIndex]) {
              return AddedTransitions;
            }

            if (edgeWithTransitionExistsInContainer(Edges_, EdgeToAdd,
                                                    Transition)) {
              return AddedTransitions;
            }
          }
          CurrentState_.InterestingVertices.emplace(TargetVertexIndex,
                                                    TargetTypeSet);
          if (!TargetVertexExists) {
            VertexData_.emplace(TargetVertexIndex, TargetTypeSet);
            VertexDepth_.push_back(CurrentState_.IterationIndex);
          }
          if (const auto [_, EdgeAdded] = Edges_.emplace(EdgeToAdd);
              EdgeAdded) {
            AddedTransitions = true;
          }

          return AddedTransitions;
        };
  };

  auto TransitionsForQuery = getTransitionsForQuery(Transitions_->Data, Query_);
  auto VertexAndTransitionsVec = constructVertexAndTransitionsPairVector(
      std::move(InterestingVertices), TransitionsForQuery,
      Transitions_->ConversionMap);
  return ranges::fold_left(
      VertexAndTransitionsVec, false,
      [this, MaybeAddEdgeFrom](bool AddedTransitions,
                               const auto &VertexAndTransitions) {
        const auto &[IndexedVertex, Transitions] = VertexAndTransitions;

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
        ToTransitions(Data.Transitions->BundeledData[Edge.TransitionIndex]);
    const auto TargetVertex = Target(Edge);
    const auto SourceVertex = Source(Edge);

    auto EdgeWeightAsString = fmt::format("{}", fmt::join(Transition, ",\n"));
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

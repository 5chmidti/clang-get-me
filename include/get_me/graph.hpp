#ifndef get_me_include_get_me_graph_hpp
#define get_me_include_get_me_graph_hpp

#include <compare>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_selectors.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/properties.hpp>

#include "get_me/indexed_graph_sets.hpp"
#include "get_me/query.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/concepts.hpp"

namespace clang {
class CXXRecordDecl;
class FieldDecl;
class FunctionDecl;
class VarDecl;
} // namespace clang

class Config;

using GraphType =
    boost::adjacency_list<boost::listS, boost::vecS, boost::directedS,
                          boost::no_property,
                          boost::property<boost::edge_index_t, size_t>>;

static_assert(
    std::is_same_v<boost::allow_parallel_edge_tag,
                   boost::graph_traits<GraphType>::edge_parallel_category>,
    "GraphType is required to allow parallel edges");

using EdgeDescriptor = typename boost::graph_traits<GraphType>::edge_descriptor;

using VertexDescriptor =
    typename boost::graph_traits<GraphType>::vertex_descriptor;

struct GraphData {
  using EdgeWeightType = TransitionType;
  using VertexDataType = TypeSet;
  using EdgeType = std::pair<VertexDescriptor, VertexDescriptor>;

  // edges
  std::vector<EdgeType> Edges{};

  // index property of an edge, allows mapping other properties (e.g. weight)
  std::vector<size_t> EdgeIndices{};
  // all possible edge weights
  std::vector<EdgeWeightType> EdgeWeights{};

  // vertices
  std::vector<VertexDataType> VertexData{};
};

inline constexpr auto Source = []<typename T>(const T &Edge)
  requires IsAnyOf<T, EdgeDescriptor, GraphData::EdgeType>
{
  if constexpr (std::same_as<T, EdgeDescriptor>) {
    return Edge.m_source;
  } else if constexpr (std::same_as<T, GraphData::EdgeType>) {
    return std::get<0>(Edge);
  }
};

inline constexpr auto Target = []<typename T>(const T &Edge)
  requires IsAnyOf<T, EdgeDescriptor, GraphData::EdgeType>
{
  if constexpr (std::same_as<T, EdgeDescriptor>) {
    return Edge.m_target;
  } else if constexpr (std::same_as<T, GraphData::EdgeType>) {
    return std::get<1>(Edge);
  }
};

[[nodiscard]] bool edgeWithTransitionExistsInContainer(
    const indexed_set<GraphData::EdgeType> &Edges,
    const GraphData::EdgeType &EdgeToAdd, const TransitionType &Transition,
    const std::vector<GraphData::EdgeWeightType> &EdgeWeights);

class GraphBuilder {
public:
  using VertexType = TypeSet;
  using VertexSet = indexed_set<VertexType>;

  explicit GraphBuilder(QueryType Query)
      : Query_(std::move(Query)),
        TransitionsForQuery_{Query_.getTransitionsForQuery()},
        VertexData_{{0U, Query_.getQueriedType()}},
        CurrentState_{0U, VertexData_} {}

  void build();
  [[nodiscard]] bool buildStep();
  [[nodiscard]] bool buildStepFor(VertexDescriptor Vertex);
  [[nodiscard]] bool buildStepFor(const VertexType &InterestingVertex);
  [[nodiscard]] bool buildStepFor(VertexSet InterestingVertices);

  [[nodiscard]] std::pair<GraphType, GraphData> commit();

private:
  struct StepState {
    size_t IterationIndex{};
    VertexSet InterestingVertices{};
  };

  [[nodiscard]] auto
  maybeAddEdgeFrom(const indexed_value<VertexType> &IndexedSourceVertex) {
    return
        [this, &IndexedSourceVertex](
            bool AddedTransitions,
            const std::pair<TransitionType, TypeSet> &TransitionAndTargetTS) {
          const auto &[Transition, TargetTypeSet] = TransitionAndTargetTS;
          auto TargetVertexIter = VertexData_.find(TargetTypeSet);
          const auto TargetVertexExists = TargetVertexIter != VertexData_.end();
          const auto TargetVertexIndex = TargetVertexExists
                                             ? Index(*TargetVertexIter)
                                             : VertexData_.size();

          const auto EdgeToAdd = GraphData::EdgeType{Index(IndexedSourceVertex),
                                                     TargetVertexIndex};

          if (TargetVertexExists &&
              edgeWithTransitionExistsInContainer(EdgesData_, EdgeToAdd,
                                                  Transition, EdgeWeights_)) {
            return AddedTransitions;
          }

          if (TargetVertexExists) {
            CurrentState_.InterestingVertices.emplace(*TargetVertexIter);
          } else {
            CurrentState_.InterestingVertices.emplace(TargetVertexIndex,
                                                      TargetTypeSet);
            VertexData_.emplace(TargetVertexIndex, TargetTypeSet);
          }
          if (const auto [_, EdgeAdded] =
                  EdgesData_.emplace(EdgesData_.size(), EdgeToAdd);
              EdgeAdded) {
            EdgeWeights_.push_back(Transition);
            AddedTransitions = true;
          }
          return AddedTransitions;
        };
  }

  QueryType Query_;
  TransitionCollector TransitionsForQuery_{};
  VertexSet VertexData_{};
  indexed_set<GraphData::EdgeType> EdgesData_{};
  std::vector<GraphData::EdgeWeightType> EdgeWeights_{};

  StepState CurrentState_{};
};

[[nodiscard]] std::pair<GraphType, GraphData>
createGraph(const QueryType &Query);

[[nodiscard]] TransitionCollector
getTypeSetTransitionData(const TransitionCollector &Collector);

[[nodiscard]] VertexDescriptor
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const TypeSet &QueriedType);

#endif

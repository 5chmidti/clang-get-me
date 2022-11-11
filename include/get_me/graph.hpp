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

namespace clang {
class CXXRecordDecl;
class FieldDecl;
class FunctionDecl;
class VarDecl;
} // namespace clang

struct Config;

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

class GraphBuilder {
public:
  explicit GraphBuilder(QueryType Query)
      : Query(std::move(Query)),
        TransitionsForQuery{this->Query.getTransitionsForQuery()},
        VertexData{{0U, this->Query.getQueriedType()}}, CurrentState{
                                                            0U, VertexData} {}

  void build();
  [[nodiscard]] bool buildStep();
  [[nodiscard]] bool buildStepFor(VertexDescriptor Vertex);
  [[nodiscard]] bool buildStepFor(const TypeSet &InterestingVertex);
  [[nodiscard]] bool
  buildStepFor(const indexed_set<TypeSet> &InterestingVertices);

  [[nodiscard]] std::pair<GraphType, GraphData> commit();

private:
  struct StepState {
    size_t IterationIndex{};
    indexed_set<TypeSet> InterestingVertices{};
  };

  QueryType Query;
  TransitionCollector TransitionsForQuery{};
  indexed_set<TypeSet> VertexData{};
  indexed_set<GraphData::EdgeType> EdgesData{};
  std::vector<GraphData::EdgeWeightType> EdgeWeights{};

  StepState CurrentState{};
};

[[nodiscard]] std::pair<GraphType, GraphData>
createGraph(const QueryType &Query);

[[nodiscard]] TransitionCollector
getTypeSetTransitionData(const TransitionCollector &Collector);

[[nodiscard]] std::optional<VertexDescriptor>
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const TypeSet &QueriedType);

#endif

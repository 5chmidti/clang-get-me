#ifndef get_me_lib_get_me_include_get_me_graph_hpp
#define get_me_lib_get_me_include_get_me_graph_hpp

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <boost/algorithm/string/replace.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_selectors.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/properties.hpp>
#include <boost/pending/property.hpp>
#include <fmt/ranges.h>
#include <range/v3/algorithm/fold_left.hpp>
#include <range/v3/view/indices.hpp>

#include "get_me/indexed_set.hpp"
#include "get_me/query.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/concepts.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/front.hpp"
#include "support/ranges/functional.hpp"
#include "support/ranges/ranges.hpp"

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
  using VertexDataType = TypeSet;
  using EdgeType = std::pair<VertexDescriptor, VertexDescriptor>;

  GraphData(std::vector<VertexDataType> VertexData,
            std::vector<size_t> VertexDepth, std::vector<EdgeType> Edges,
            std::vector<TransitionType> EdgeTransitions);

  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)

  // vertices
  std::vector<VertexDataType> VertexData;

  // depth the vertex was first visited
  std::vector<size_t> VertexDepth;

  // edges
  std::vector<EdgeType> Edges;

  // index property of an edge, allows mapping other properties (e.g. weight)
  std::vector<size_t> EdgeIndices;
  // all possible edge transitions
  std::vector<TransitionType> EdgeTransitions;

  // graph
  GraphType Graph;
  // NOLINTEND(misc-non-private-member-variables-in-classes)
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

template <> class fmt::formatter<EdgeDescriptor> {
public:
  [[nodiscard]] constexpr auto parse(format_parse_context &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const EdgeDescriptor &Edge,
                            FormatContext &Ctx) const -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "{}",
                          std::pair{Source(Edge), Target(Edge)});
  }
};

template <> class fmt::formatter<GraphData> {
public:
  [[nodiscard]] constexpr auto parse(format_parse_context &Ctx)
      -> decltype(Ctx.begin()) {
    const auto *Iter = Ctx.begin();
    const auto *const End = Ctx.end();
    if (Iter != End && *Iter == 'd') {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      Presentation_ = *Iter++;
      Ctx.advance_to(Iter);
    }
    return Iter;
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const GraphData &Val, FormatContext &Ctx) const
      -> decltype(Ctx.out()) {
    switch (Presentation_) {
    case 'd':
      return fmt::format_to(Ctx.out(), "{}", toDotFormat(Val));
    }
    return fmt::format_to(Ctx.out(), "{}", Val);
  }

private:
  [[nodiscard]] static std::string toDotFormat(const GraphData &Data);

  char Presentation_{};
};

[[nodiscard]] bool edgeWithTransitionExistsInContainer(
    const indexed_set<GraphData::EdgeType> &Edges,
    const GraphData::EdgeType &EdgeToAdd, const TransitionType &Transition,
    const std::vector<TransitionType> &EdgeTransitions);

class GraphBuilder {
public:
  using VertexType = TypeSet;
  using VertexSet = indexed_set<VertexType>;

  explicit GraphBuilder(const TransitionCollector &Transitions,
                        const TypeSetValueType &Query, const Config &Conf)
      : TransitionsForQuery_{getTransitionsForQuery(Transitions, Query)},
        VertexData_{{0U, TypeSet{Query}}},
        VertexDepth_{{0U, 0U}},
        Conf_{Conf},
        CurrentState_{0U, VertexData_} {}

  void build();
  [[nodiscard]] bool buildStep();
  [[nodiscard]] bool buildStepFor(VertexDescriptor Vertex);
  [[nodiscard]] bool buildStepFor(const VertexType &InterestingVertex);
  [[nodiscard]] bool buildStepFor(VertexSet InterestingVertices);

  [[nodiscard]] GraphData commit();

private:
  struct StepState {
    size_t IterationIndex{};
    VertexSet InterestingVertices{};
  };

  [[nodiscard]] size_t getVertexDepth(size_t VertexIndex) const;

  [[nodiscard]] static std::int64_t
  getVertexDepthDifference(size_t SourceDepth, size_t TargetDepth);

  TransitionCollector TransitionsForQuery_{};
  VertexSet VertexData_{};
  indexed_set<size_t> VertexDepth_{};
  indexed_set<GraphData::EdgeType> EdgesData_{};
  std::vector<TransitionType> EdgeTransitions_{};
  Config Conf_{};

  StepState CurrentState_{};
};

[[nodiscard]] GraphData createGraph(const TransitionCollector &Transitions,
                                    const TypeSetValueType &Query,
                                    const Config &Conf);

[[nodiscard]] VertexDescriptor
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const TypeSetValueType &QueriedType);

#endif

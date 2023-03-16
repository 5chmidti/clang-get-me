#ifndef get_me_lib_get_me_include_get_me_graph_hpp
#define get_me_lib_get_me_include_get_me_graph_hpp

#include <cstddef>
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

  GraphData(std::vector<VertexDataType> VertexData, std::vector<EdgeType> Edges,
            std::vector<EdgeWeightType> EdgeWeights)
      : VertexData{std::move(VertexData)},
        Edges{std::move(Edges)},
        EdgeIndices{ranges::views::indices(this->Edges.size()) |
                    ranges::to_vector},
        EdgeWeights{std::move(EdgeWeights)},
        Graph{this->Edges.data(), this->Edges.data() + this->Edges.size(),
              this->EdgeIndices.data(), this->EdgeIndices.size()} {}

  // vertices
  std::vector<VertexDataType> VertexData;

  // edges
  std::vector<EdgeType> Edges;

  // index property of an edge, allows mapping other properties (e.g. weight)
  std::vector<size_t> EdgeIndices;
  // all possible edge weights
  std::vector<EdgeWeightType> EdgeWeights;

  // graph
  GraphType Graph;
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
  [[nodiscard]] static std::string toDotFormat(const GraphData &Data) {
    const auto IndexMap = boost::get(boost::edge_index, Data.Graph);

    const auto ToString = [&Data, &IndexMap](const auto &Edge) {
      const auto SourceNode = Source(Edge);
      const auto TargetNode = Target(Edge);

      const auto Transition =
          ToTransition(Data.EdgeWeights[boost::get(IndexMap, Edge)]);
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

  char Presentation_{};
};

[[nodiscard]] bool edgeWithTransitionExistsInContainer(
    const indexed_set<GraphData::EdgeType> &Edges,
    const GraphData::EdgeType &EdgeToAdd, const TransitionType &Transition,
    const std::vector<GraphData::EdgeWeightType> &EdgeWeights);

class GraphBuilder {
public:
  using VertexType = TypeSet;
  using VertexSet = indexed_set<VertexType>;

  explicit GraphBuilder(const TransitionCollector &Transitions,
                        const TypeSetValueType &Query, const Config &Conf)
      : TransitionsForQuery_{getTransitionsForQuery(Transitions, Query)},
        VertexData_{{0U, TypeSet{Query}}},
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

  TransitionCollector TransitionsForQuery_{};
  VertexSet VertexData_{};
  indexed_set<GraphData::EdgeType> EdgesData_{};
  std::vector<GraphData::EdgeWeightType> EdgeWeights_{};
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

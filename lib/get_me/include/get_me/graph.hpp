#ifndef get_me_lib_get_me_include_get_me_graph_hpp
#define get_me_lib_get_me_include_get_me_graph_hpp

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/container/flat_set.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <range/v3/algorithm/lexicographical_compare.hpp>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/functional/comparisons.hpp>
#include <range/v3/range/traits.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/indexed_set.hpp"
#include "get_me/query.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/ranges/functional.hpp"

namespace clang {
class CXXRecordDecl;
class FieldDecl;
class FunctionDecl;
class VarDecl;
} // namespace clang

class Config;

using VertexDescriptor = size_t;

using EdgeType = std::pair<size_t, size_t>;
// make size_t the index of the transition for the permutation comparator

struct TransitionEdgeType {
  EdgeType Edge{};
  size_t TransitionIndex{};

  [[nodiscard]] friend constexpr auto
  operator<=>(const TransitionEdgeType &, const TransitionEdgeType &) = default;
};

// FIXME: can combine multiple edges by storing all indices belonging to those
// edges in a vector
// this would also implement the edges as transition independent transitions
// (saves on duplicate paths and others)
// the problem with this would be that of rendering paths as strings (or just
// use views::for_each)

template <> class fmt::formatter<TransitionEdgeType> {
public:
  [[nodiscard]] constexpr auto parse(format_parse_context &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const TransitionEdgeType &Val,
                            FormatContext &Ctx) const -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "({}, {})", Val.Edge, Val.TransitionIndex);
  }
};

using PathType = std::vector<TransitionEdgeType>;

struct IsPermutationComparator {
  [[nodiscard]] bool operator()(const PathType &Lhs,
                                const PathType &Rhs) const {
    if (const auto Comp = Lhs.size() <=> Rhs.size(); std::is_neq(Comp)) {
      return std::is_lt(Comp);
    }

    return !ranges::is_permutation(Lhs, Rhs, ranges::equal_to{},
                                   &TransitionEdgeType::TransitionIndex,
                                   &TransitionEdgeType::TransitionIndex) &&
           ranges::lexicographical_compare(
               Lhs, Rhs, ranges::less{}, &TransitionEdgeType::TransitionIndex,
               &TransitionEdgeType::TransitionIndex);
  }
};

struct GraphData {
  using EdgeContainer = boost::container::flat_set<TransitionEdgeType>;
  using PathContainer =
      boost::container::flat_set<PathType, IsPermutationComparator>;

  GraphData(std::vector<TypeSet> VertexData, std::vector<size_t> VertexDepth,
            EdgeContainer Edges,
            std::shared_ptr<TransitionCollector> Transitions);

  GraphData(std::vector<TypeSet> VertexData, std::vector<size_t> VertexDepth,
            EdgeContainer Edges,
            std::shared_ptr<TransitionCollector> Transitions,
            PathContainer Paths);

  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)

  std::vector<TypeSet> VertexData;

  // depth the vertex was first visited
  std::vector<size_t> VertexDepth;

  EdgeContainer Edges;

  PathContainer Paths{};

  std::shared_ptr<TransitionCollector> Transitions{};

  // NOLINTEND(misc-non-private-member-variables-in-classes)
};

inline constexpr auto Source = []<typename T>(const T &Edge) -> VertexDescriptor
// requires IsAnyOf<T, EdgeDescriptor, EdgeType>
{
  if constexpr (requires(T Val) { Val.m_source; }) {
    return Edge.m_source;
  } else if constexpr (std::same_as<T, EdgeType>) {
    return std::get<0>(Edge);
  } else if constexpr (std::same_as<T, TransitionEdgeType>) {
    return std::get<0>(Edge.Edge);
  } else {
    static_assert(std::same_as<void, T>);
  }
};

inline constexpr auto Target = []<typename T>(const T &Edge) -> VertexDescriptor
// requires IsAnyOf<T, EdgeDescriptor, EdgeType>
{
  if constexpr (requires(T Val) { Val.m_target; }) {
    return Edge.m_target;
  } else if constexpr (std::same_as<T, EdgeType>) {
    return std::get<1>(Edge);
  } else if constexpr (std::same_as<T, TransitionEdgeType>) {
    return std::get<1>(Edge.Edge);
  } else {
    static_assert(std::same_as<void, T>);
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
    return fmt::format_to(Ctx.out(), R"(
  VertexData: {}
  VertexDepth: {}
  Edges: {}
  Paths: {})",
                          Val.VertexData, Val.VertexDepth, Val.Edges,
                          Val.Paths);
  }

private:
  [[nodiscard]] static std::string toDotFormat(const GraphData &Data);

  char Presentation_{};
};

[[nodiscard]] std::vector<VertexDescriptor> getRootVertices(GraphData &Data);

[[nodiscard]] std::vector<VertexDescriptor> getLeafVertices(GraphData &Data);

template <typename RangeType>
  requires std::same_as<ranges::range_value_t<RangeType>, PathType>
[[nodiscard]] auto toString(RangeType &&Paths, const GraphData &Data) {
  const auto FormatPath = [&Data](const PathType &Path) {
    const auto GetTransition = [&Data](const TransitionEdgeType &Edge) {
      return Value(Data.Transitions->FlatData[Edge.TransitionIndex]);
    };

    return fmt::format(
        "{}", fmt::join(Path | ranges::views::transform(GetTransition), ", "));
  };

  return std::forward<RangeType>(Paths) | ranges::views::transform(FormatPath);
};

class GraphBuilder {
public:
  using VertexType = TypeSet;
  using VertexSet = indexed_set<VertexType>;

  explicit GraphBuilder(std::shared_ptr<TransitionCollector> Transitions,
                        const TypeSetValueType &Query,
                        std::shared_ptr<Config> Conf)
      : Transitions_{std::move(Transitions)},
        Query_{Query},
        VertexData_{{0U, TypeSet{Query}}, {1U, TypeSet{}}},
        VertexDepth_{{0U, 0U}, {1U, 0U}},
        Conf_{std::move(Conf)},
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

  std::shared_ptr<TransitionCollector> Transitions_;
  TypeSetValueType Query_;
  VertexSet VertexData_{};
  indexed_set<size_t> VertexDepth_{};
  GraphData::EdgeContainer Edges_{};
  std::shared_ptr<Config> Conf_{};
  boost::container::flat_set<PathType, IsPermutationComparator> Paths_{};

  StepState CurrentState_{};
};

[[nodiscard]] GraphData
runGraphBuilding(const std::shared_ptr<TransitionCollector> &Transitions,
                 const TypeSetValueType &Query, std::shared_ptr<Config> Conf);

[[nodiscard]] VertexDescriptor
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const TypeSetValueType &QueriedType);

#endif

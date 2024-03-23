#ifndef get_me_lib_get_me_include_get_me_graph_hpp
#define get_me_lib_get_me_include_get_me_graph_hpp

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/container/flat_set.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <range/v3/algorithm/lexicographical_compare.hpp>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/functional/bind_back.hpp>
#include <range/v3/functional/comparisons.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/range/traits.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/indexed_set.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"

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
  // NOLINTBEGIN(readability-convert-member-functions-to-static)
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &Ctx) {
    return Ctx.begin();
  }

  [[nodiscard]] format_context::iterator format(const TransitionEdgeType &Val,
                                                format_context &Ctx) const {
    return fmt::format_to(Ctx.out(), "({}, {})", Val.Edge, Val.TransitionIndex);
  }
  // NOLINTEND(readability-convert-member-functions-to-static)
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

using PathContainer =
    boost::container::flat_set<PathType, IsPermutationComparator>;

struct FlatPathEdge {
  FlatPathEdge(EdgeType Edge, FlatTransitionType FlatTransition)
      : Edge(std::move(Edge)),
        FlatTransition(std::move(FlatTransition)) {}

  EdgeType Edge;
  FlatTransitionType FlatTransition;
};

template <> class fmt::formatter<FlatPathEdge> {
public:
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &Ctx) {
    return Ctx.begin();
  }

  [[nodiscard]] format_context::iterator format(const FlatPathEdge &Val,
                                                format_context &Ctx) const {
    return fmt::format_to(Ctx.out(), "({}, {})", Val.Edge, Val.FlatTransition);
  }
};

struct GraphData {
  using EdgeContainer = boost::container::flat_set<TransitionEdgeType>;

  GraphData(std::vector<TypeSet> VertexData, std::vector<size_t> VertexDepth,
            EdgeContainer Edges, std::shared_ptr<TransitionData> Transitions,
            std::shared_ptr<Config> Conf);

  GraphData(std::vector<TypeSet> VertexData, std::vector<size_t> VertexDepth,
            EdgeContainer Edges, std::shared_ptr<TransitionData> Transitions,
            PathContainer Paths, std::shared_ptr<Config> Conf);

  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)

  std::vector<TypeSet> VertexData;

  // depth the vertex was first visited
  std::vector<size_t> VertexDepth;

  EdgeContainer Edges;

  std::shared_ptr<TransitionData> Transitions{};

  std::shared_ptr<Config> Conf{};
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
  // NOLINTBEGIN(readability-convert-member-functions-to-static)
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &Ctx) {
    const auto *Iter = Ctx.begin();
    const auto *const End = Ctx.end();
    if (Iter != End && (*Iter == 'd' || *Iter == 's')) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      Presentation_ = *Iter++;
      Ctx.advance_to(Iter);
    }
    return Iter;
  }

  [[nodiscard]] format_context::iterator format(const GraphData &Val,
                                                format_context &Ctx) const {
    switch (Presentation_) {
    case 'd':
      return fmt::format_to(Ctx.out(), "{}", toDotFormat(Val));
    case 's':
      return fmt::format_to(
          Ctx.out(), R"(
  VertexData: {}
  VertexDepth: {}
  Edges: {}
  Transitions: {}
  )",
          ranges::size(Val.VertexData), ranges::size(Val.VertexDepth),
          ranges::size(Val.Edges), ranges::size(Val.Transitions->Data));
    }
    return fmt::format_to(Ctx.out(), R"(
  VertexData: {}
  VertexDepth: {}
  Edges: {}
  Transitions:
    {}
  )",
                          Val.VertexData, Val.VertexDepth, Val.Edges,
                          fmt::join(Val.Transitions->Data, "\n\t"));
  }
  // NOLINTEND(readability-convert-member-functions-to-static)

private:
  [[nodiscard]] static std::string toDotFormat(const GraphData &Data);

  char Presentation_{};
};

[[nodiscard]] std::vector<VertexDescriptor>
getRootVertices(const GraphData &Data);

[[nodiscard]] std::vector<VertexDescriptor>
getLeafVertices(const GraphData &Data);

namespace detail {
[[nodiscard]] inline std::string
formatTransition(const TransitionEdgeType &Edge, const GraphData &Data) {
  const auto Transition = Data.Transitions->BundeledData[Edge.TransitionIndex];
  return fmt::format("({}, {}, {})", ToAcquired(Transition),
                     ToTransitions(Transition) | ranges::views::values,
                     ToRequired(Transition));
};

[[nodiscard]] inline std::string formatPath(const PathType &Path,
                                            const GraphData &Data) {
  return fmt::format(
      "{}", fmt::join(Path | ranges::views::transform(
                                 ranges::bind_back(formatTransition, Data)),
                      ", "));
};
} // namespace detail

template <typename RangeType>
  requires std::same_as<ranges::range_value_t<RangeType>, PathType>
[[nodiscard]] auto formatPaths(const RangeType &Paths, const GraphData &Data) {
  return Paths |
         ranges::views::transform(ranges::bind_back(detail::formatPath, Data));
}

[[nodiscard]] std::vector<std::vector<FlatPathEdge>>
expandAndFlattenPath(const PathType &Path, const GraphData &Data);

template <typename RangeType>
  requires std::same_as<ranges::range_value_t<RangeType>, PathType>
[[nodiscard]] auto toStringExpanded(const RangeType &Paths,
                                    const GraphData &Data) {
  const auto FormatPath = [](const std::vector<FlatPathEdge> &FlatPath) {
    return fmt::format("{}",
                       fmt::join(FlatPath | ranges::views::transform(
                                                &FlatPathEdge::FlatTransition),
                                 ", "));
  };

  return Paths |
         ranges::views::for_each(
             ranges::bind_back(expandAndFlattenPath, Data)) |
         ranges::views::transform(FormatPath);
}

class GraphBuilder {
public:
  using VertexType = TypeSet;
  using VertexSet = indexed_set<VertexType>;

  explicit GraphBuilder(std::shared_ptr<TransitionData> Transitions,
                        TypeSet Query, std::shared_ptr<Config> Conf);

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

  class GraphBuilderImpl;

  [[nodiscard]] static std::int64_t
  getVertexDepthDifference(size_t SourceDepth, size_t TargetDepth);

  [[nodiscard]] bool
  isEmptyTargetTS(const VertexSet::key_type::first_type VertexIndex) const {
    return VertexIndex == EmptyTsIndex_;
  }

  std::shared_ptr<TransitionData> Transitions_;
  TypeSet Query_;
  VertexSet::value_type::first_type EmptyTsIndex_;
  VertexSet VertexData_{};
  std::vector<size_t> VertexDepth_{};
  GraphData::EdgeContainer Edges_{};
  std::shared_ptr<Config> Conf_{};

  StepState CurrentState_{};

  std::unique_ptr<GraphBuilderImpl> Impl_{};
};

[[nodiscard]] GraphData
runGraphBuilding(const std::shared_ptr<TransitionData> &Transitions,
                 const TypeSet &Query, std::shared_ptr<Config> Conf);

#endif

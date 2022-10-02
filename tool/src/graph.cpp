#include "get_me/graph.hpp"

#include <compare>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <stack>
#include <string>
#include <utility>

#include <boost/algorithm/string.hpp>
#include <boost/graph/detail/adjacency_list.hpp>
#include <boost/graph/detail/edge.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/property_map/property_map.hpp>
#include <clang/AST/DeclCXX.h>
#include <fmt/chrono.h>
#include <llvm/Support/Casting.h>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/binary_search.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/algorithm/unique.hpp>
#include <range/v3/functional/comparisons.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/subrange.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"
#include "get_me/type_set.hpp"

[[nodiscard]] static auto createIsValidPathPredicate(const Config &Conf) {
  return [MaxPathLength = Conf.MaxPathLength.value_or(
              std::numeric_limits<std::size_t>::max())](
             const PathType &CurrentPath) {
    return MaxPathLength >= CurrentPath.size();
  };
}

[[nodiscard]] static auto createContinuePathSearchPredicate(
    const Config &Conf, const ranges::sized_range auto &CurrentPaths) {
  return [&CurrentPaths, MaxPathCount = Conf.MaxPathCount.value_or(
                             std::numeric_limits<std::size_t>::max())]() {
    return MaxPathCount > ranges::size(CurrentPaths);
  };
}

// FIXME: there exist paths that contain edges with aliased types and edges with
// their base types, basically creating redundant/non-optimal paths
// FIXME: don't produce paths that end up with the queried type
std::vector<PathType> pathTraversal(const GraphType &Graph,
                                    const GraphData &Data, const Config &Conf,
                                    const VertexDescriptor SourceVertex) {
  using possible_path_type = std::pair<VertexDescriptor, EdgeDescriptor>;

  PathType CurrentPath{};
  const auto IsPermutation = [&Graph, &Data](const PathType &Lhs,
                                             const PathType &Rhs) {
    const auto IndexMap = get(boost::edge_index, Graph);
    const auto ToEdgeWeight = [&IndexMap, &Data](const EdgeDescriptor &Edge) {
      return Data.EdgeWeights[get(IndexMap, Edge)];
    };
    if (const auto Comp = Lhs.size() <=> Rhs.size(); std::is_neq(Comp)) {
      return std::is_lt(Comp);
    }
    return !ranges::is_permutation(Lhs, Rhs, ranges::equal_to{}, ToEdgeWeight,
                                   ToEdgeWeight);
  };
  auto Paths = std::set(std::initializer_list<PathType>{}, IsPermutation);
  std::stack<possible_path_type> EdgesStack{};

  const auto AddToStackFactory = [&EdgesStack](const VertexDescriptor Vertex) {
    return [&EdgesStack, Vertex](const EdgeDescriptor Edge) {
      EdgesStack.emplace(Vertex, Edge);
    };
  };

  ranges::for_each(toRange(out_edges(SourceVertex, Graph)),
                   AddToStackFactory(SourceVertex));

  const auto CurrentPathContainsTargetOfEdge =
      [&CurrentPath, &Graph](const EdgeDescriptor Edge) {
        const auto HasTargetEdge = [&Graph, TargetVertex = target(Edge, Graph)](
                                       const EdgeDescriptor &EdgeInPath) {
          return source(EdgeInPath, Graph) == TargetVertex ||
                 target(EdgeInPath, Graph) == TargetVertex;
        };
        return !ranges::any_of(CurrentPath, HasTargetEdge);
      };
  const auto AddOutEdgesOfVertexToStack =
      [&Graph, &AddToStackFactory,
       &CurrentPathContainsTargetOfEdge](const VertexDescriptor Vertex) {
        const auto Vec = ranges::to_vector(toRange(out_edges(Vertex, Graph)));
        auto OutEdgesRange =
            ranges::views::filter(Vec, CurrentPathContainsTargetOfEdge);
        if (OutEdgesRange.empty()) {
          return false;
        }

        ranges::for_each(OutEdgesRange, AddToStackFactory(Vertex));

        return true;
      };

  const auto IsValidPath = createIsValidPathPredicate(Conf);
  const auto ContinuePathSearch =
      createContinuePathSearchPredicate(Conf, Paths);

  VertexDescriptor CurrentVertex{};
  VertexDescriptor PrevTarget{SourceVertex};
  const auto RequiresRollback = [&CurrentPath,
                                 &PrevTarget](const VertexDescriptor Src) {
    return !CurrentPath.empty() && PrevTarget != Src;
  };
  while (!EdgesStack.empty() && ContinuePathSearch()) {
    const auto [Src, Edge] = EdgesStack.top();
    EdgesStack.pop();

    if (ranges::contains(CurrentPath, Edge)) {
      spdlog::error("skipping visiting edge already in path: {}", Edge);
      continue;
    }

    if (RequiresRollback(Src)) {
      // visiting an edge whose source is not the target of the previous edge.
      // the current path has to be reverted until the new edge can be added to
      // the path
      // remove edges that were added after the path got to src
      const auto GetEdgeSource = [&Graph](const EdgeDescriptor &Val) {
        return source(Val, Graph);
      };
      CurrentPath.erase(ranges::find(CurrentPath, Src, GetEdgeSource),
                        CurrentPath.end());
    }
    PrevTarget = target(Edge, Graph);

    CurrentPath.emplace_back(Edge);
    CurrentVertex = target(Edge, Graph);

    if (!IsValidPath(CurrentPath)) {
      continue;
    }

    if (const auto IsFinalVertexInPath =
            !AddOutEdgesOfVertexToStack(CurrentVertex);
        IsFinalVertexInPath) {
      Paths.insert(CurrentPath);
    }
  }

  return ranges::to_vector(Paths);
}

std::vector<PathType> independentPaths(const std::vector<PathType> &Paths,
                                       const GraphType &Graph,
                                       const GraphData &Data) {
  assert(std::ranges::is_sorted(Paths, std::less{}, &PathType::size));

  const auto ToEdgeWeight = [IndexMap = get(boost::edge_index, Graph),
                             EdgeWeights =
                                 Data.EdgeWeights](const EdgeDescriptor &Edge) {
    return EdgeWeights[get(IndexMap, Edge)];
  };

  auto Res = ranges::to_vector(ranges::views::join(
      Paths |
      ranges::views::chunk_by([](const PathType &Lhs, const PathType &Rhs) {
        return Lhs.size() == Rhs.size();
      }) |
      ranges::views::transform([&ToEdgeWeight](
                                   const ranges::forward_range auto Range) {
        std::vector<PathType> UniquePaths{};

        for (const auto &Path : Range) {
          if (const auto EquivalentPathContainedInResult = ranges::any_of(
                  UniquePaths,
                  [&ToEdgeWeight, &Path](const PathType &ExistingPath) {
                    return ranges::is_permutation(Path, ExistingPath,
                                                  ranges::equal_to{},
                                                  ToEdgeWeight, ToEdgeWeight);
                  });
              EquivalentPathContainedInResult) {
            continue;
          }

          UniquePaths.push_back(Path);
        }

        return UniquePaths;
      })));
  return Res;
}

[[nodiscard]] static auto matchesNamePredicateFactory(std::string Name) {
  return [Name = std::move(Name)](const TypeSetValueType &Val) {
    return std::visit(
        Overloaded{
            [&Name](const clang::Type *const Type) {
              const auto QType = clang::QualType(Type, 0);
              const auto TypeAsString = [&QType]() {
                auto QTypeAsString = QType.getAsString();
                boost::erase_all(QTypeAsString, "struct");
                boost::erase_all(QTypeAsString, "class");
                boost::trim(QTypeAsString);
                return QTypeAsString;
              }();
              const auto EquivalentName = TypeAsString == Name;
              if (!EquivalentName &&
                  (TypeAsString.find(Name) != std::string::npos)) {
                spdlog::trace(
                    "matchesName(QualType): no match for close match: {} vs {}",
                    TypeAsString, Name);
              }
              return EquivalentName;
            },
            [](const ArithmeticType &) { return false; }},
        Val);
  };
}

static void initializeVertexDataWithQueried(
    const TransitionCollector &TypeSetTransitionData, GraphData &Data,
    const std::string &TypeName) {
  const auto ToAcquired = [](const TransitionType &Val) {
    return std::get<0>(Val);
  };
  const auto MatchesQueriedName = matchesNamePredicateFactory(TypeName);
  ranges::transform(ranges::views::filter(
                        TypeSetTransitionData,
                        [&Data, &MatchesQueriedName](const TypeSet &Acquired) {
                          return ranges::any_of(Acquired, MatchesQueriedName) &&
                                 !ranges::contains(Data.VertexData, Acquired);
                        },
                        ToAcquired),
                    std::back_inserter(Data.VertexData), ToAcquired);
}

[[nodiscard]] static auto isSubsetPredicateFactory(const TypeSet &Subset) {
  return
      [&Subset](const TypeSet &Superset) { return isSubset(Superset, Subset); };
}

[[nodiscard]] static TypeSet merge(TypeSet Lhs, TypeSet Rhs) {
  Lhs.merge(std::move(Rhs));
  return Lhs;
}

[[nodiscard]] static TypeSet subtract(const TypeSet &Lhs, const TypeSet &Rhs) {
  TypeSet Res{};
  ranges::set_difference(Lhs, Rhs, std::inserter(Res, Res.end()), std::less{});
  return Res;
}

using indexed_vertex_type = std::pair<TypeSet, size_t>;
struct VertexSetComparator {
  using is_transparent = void;
  [[nodiscard]] bool operator()(const indexed_vertex_type &Lhs,
                                const indexed_vertex_type &Rhs) const {
    return Lhs.first < Rhs.first;
  }
  [[nodiscard]] bool operator()(const indexed_vertex_type &Lhs,
                                const TypeSet &Rhs) const {
    return Lhs.first < Rhs;
  }
  [[nodiscard]] bool operator()(const TypeSet &Lhs,
                                const indexed_vertex_type &Rhs) const {
    return Lhs < Rhs.first;
  }
};
using vertex_set = std::set<indexed_vertex_type, VertexSetComparator>;

using indexed_edge_type = std::pair<GraphData::EdgeType, size_t>;
struct EdgeSetComparator {
  using is_transparent = void;
  [[nodiscard]] bool operator()(const indexed_edge_type &Lhs,
                                const indexed_edge_type &Rhs) const {
    return Lhs < Rhs;
  }
  [[nodiscard]] bool operator()(const indexed_edge_type &Lhs,
                                const GraphData::EdgeType &Rhs) const {
    return Lhs.first < Rhs;
  }
  [[nodiscard]] bool operator()(const GraphData::EdgeType &Lhs,
                                const indexed_edge_type &Rhs) const {
    return Lhs < Rhs.first;
  }
};
using edge_set = std::set<indexed_edge_type, EdgeSetComparator>;

[[nodiscard]] static std::pair<bool, vertex_set::const_iterator>
findNewRequiredTypeSet(const TypeSet &NewRequiredTypeSet,
                       const vertex_set &TemporaryVertexData,
                       const vertex_set &VertexData) {
  const auto GetExistingOpt = [&NewRequiredTypeSet](const auto &Container)
      -> std::optional<typename vertex_set::const_iterator> {
    if (auto Iter = Container.find(NewRequiredTypeSet);
        Iter != Container.end()) {
      return Iter;
    }
    return std::nullopt;
  };
  if (const auto Existing = GetExistingOpt(TemporaryVertexData); Existing) {
    return {true, Existing.value()};
  }
  if (const auto Existing = GetExistingOpt(VertexData)) {
    return {true, Existing.value()};
  }
  return {false, TemporaryVertexData.end()};
}

[[nodiscard]] static bool transitionToAddAlreadyExistsInContainer(
    const edge_set &Container, const GraphData::EdgeType &EdgeToAdd,
    const TransitionType &Transition,
    const std::vector<GraphData::EdgeWeightType> &EdgeWeights) {
  const auto LowerBound = Container.lower_bound(EdgeToAdd);
  const auto UpperBound = Container.upper_bound(EdgeToAdd);
  return ranges::contains(
      ranges::subrange(LowerBound, UpperBound) |
          ranges::views::transform(
              [&EdgeWeights](const indexed_edge_type &IndexedEdge) {
                return std::pair{IndexedEdge.first,
                                 EdgeWeights[IndexedEdge.second]};
              }),
      std::pair{EdgeToAdd, Transition});
}

static void buildGraph(const TransitionCollector &TypeSetTransitionData,
                       GraphData &Data, const Config &Conf) {
  const auto QueriedTypes = Data.VertexData;

  auto VertexData = ranges::to<vertex_set>(ranges::views::zip(
      Data.VertexData, ranges::views::iota(static_cast<size_t>(0U))));

  edge_set EdgesData{};

  size_t IterationCount = 0U;
  auto TypeSetsOfInterest = VertexData;

  Data.VertexData.emplace_back();

  const auto MaxGraphDepth =
      Conf.MaxGraphDepth.value_or(std::numeric_limits<std::size_t>::max());
  for (bool AddedTransitions = true;
       AddedTransitions && IterationCount < MaxGraphDepth; ++IterationCount) {
    vertex_set TemporaryVertexData{};
    edge_set TemporaryEdgeData{};
    vertex_set NewTypeSetsOfInterest{};
    AddedTransitions = false;
    size_t TransitionCounter = 0U;
    spdlog::trace("{:=^50}", "");
    const auto TransitionWithInterestingAcquiredTypeSet =
        [&TypeSetsOfInterest](const TransitionType &Val) {
          const auto &Acquired = std::get<0>(Val);
          return ranges::any_of(TypeSetsOfInterest,
                                isSubsetPredicateFactory(Acquired),
                                &indexed_vertex_type::first);
        };

    for (const auto FilteredTypeSetTransitionData = ranges::to_vector(
             TypeSetTransitionData |
             ranges::views::filter(TransitionWithInterestingAcquiredTypeSet));
         const auto &Transition : FilteredTypeSetTransitionData) {
      const auto &[AcquiredTypeSet, Function, RequiredTypeSet] = Transition;
      ++TransitionCounter;
      for (const auto FilteredTypeSetsOfInterest = ranges::to_vector(
               TypeSetsOfInterest |
               ranges::views::filter(isSubsetPredicateFactory(AcquiredTypeSet),
                                     &indexed_vertex_type::first));
           const auto &[VertexTypeSet, SourceTypeSetIndex] :
           FilteredTypeSetsOfInterest) {
        auto NewRequiredTypeSet =
            merge(subtract(VertexTypeSet, AcquiredTypeSet), RequiredTypeSet);
        if (ranges::any_of(QueriedTypes, [&](const TypeSet &QueriedTypeSet) {
              return isSubset(NewRequiredTypeSet, QueriedTypeSet);
            })) {
          continue;
        }
        const auto [NewRequiredTypeSetExists, NewRequiredTypeSetIter] =
            findNewRequiredTypeSet(NewRequiredTypeSet, TemporaryVertexData,
                                   VertexData);

        if (NewRequiredTypeSetExists) {
          NewTypeSetsOfInterest.emplace(*NewRequiredTypeSetIter);
        }
        const auto NewRequiredTypeSetIndex =
            NewRequiredTypeSetExists
                ? NewRequiredTypeSetIter->second
                : VertexData.size() + TemporaryVertexData.size();

        const auto EdgeToAdd =
            std::pair{SourceTypeSetIndex, NewRequiredTypeSetIndex};

        if (NewRequiredTypeSetExists &&
            (transitionToAddAlreadyExistsInContainer(
                 TemporaryEdgeData, EdgeToAdd, Transition, Data.EdgeWeights) ||
             transitionToAddAlreadyExistsInContainer(
                 EdgesData, EdgeToAdd, Transition, Data.EdgeWeights))) {
          continue;
        }

        if (!NewRequiredTypeSetExists) {
          NewTypeSetsOfInterest.emplace(NewRequiredTypeSet,
                                        NewRequiredTypeSetIndex);
          TemporaryVertexData.emplace(std::move(NewRequiredTypeSet),
                                      NewRequiredTypeSetIndex);
        }
        if (const auto [_, EdgeAdded] = TemporaryEdgeData.emplace(
                EdgeToAdd, EdgesData.size() + TemporaryEdgeData.size());
            EdgeAdded) {
          Data.EdgeWeights.push_back(Transition);
          AddedTransitions = true;
        }
      }
      spdlog::trace("#{} transition #{} (|V| = {}(+{}), |E| = {}(+{})): {}",
                    IterationCount, TransitionCounter,
                    VertexData.size() + TemporaryVertexData.size(),
                    TemporaryVertexData.size(),
                    EdgesData.size() + TemporaryEdgeData.size(),
                    TemporaryEdgeData.size(), Transition);
    }

    VertexData.merge(std::move(TemporaryVertexData));
    TemporaryVertexData.clear();

    EdgesData.merge(std::move(TemporaryEdgeData));
    TemporaryEdgeData.clear();

    TypeSetsOfInterest = std::move(NewTypeSetsOfInterest);
  }
  spdlog::trace("{:=^50}", "");

  const auto GetSortedIndexedData =
      []<ranges::range RangeType>(const RangeType &Range) {
        auto DataToSort = ranges::to_vector(Range);
        ranges::sort(DataToSort, std::less{},
                     &ranges::range_value_t<RangeType>::second);
        return DataToSort;
      };

  auto SortedVertexData = GetSortedIndexedData(VertexData);
  Data.VertexData = ranges::to_vector(
      SortedVertexData | ranges::views::transform(&indexed_vertex_type::first));

  auto SortedEdgeData = GetSortedIndexedData(EdgesData);
  Data.Edges = ranges::to_vector(
      SortedEdgeData | ranges::views::transform(&indexed_edge_type::first));

  Data.EdgeIndices = ranges::to_vector(
      ranges::views::iota(static_cast<size_t>(0U), Data.Edges.size()));
}

std::pair<GraphType, GraphData>
createGraph(const TransitionCollector &TypeSetTransitionData,
            const std::string &TypeName, const Config &Conf) {
  GraphData Data{};
  initializeVertexDataWithQueried(TypeSetTransitionData, Data, TypeName);

  spdlog::trace("initial GraphData.VertexData: {}", Data.VertexData);

  buildGraph(TypeSetTransitionData, Data, Conf);

  return {GraphType(Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
                    Data.EdgeIndices.data(), Data.EdgeIndices.size()),
          Data};
}

std::optional<VertexDescriptor>
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const std::string &TypeName) {
  // FIXME: improve queried type matching:
  // - allow matching mutiple to get around QualType vs NamedDecl problem
  // FIXME: only getting the 'A' type, not the & qualified
  const auto SourceVertex =
      ranges::find_if(Data.VertexData, [&TypeName](const TypeSet &TSet) {
        return TSet.end() !=
               ranges::find_if(TSet, matchesNamePredicateFactory(TypeName));
      });

  if (SourceVertex == Data.VertexData.end()) {
    spdlog::error("found no type matching {} in {}", TypeName, Data.VertexData);
    return std::nullopt;
  }
  return static_cast<VertexDescriptor>(
      std::distance(Data.VertexData.begin(), SourceVertex));
}

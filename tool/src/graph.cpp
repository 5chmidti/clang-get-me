#include "get_me/graph.hpp"

#include <functional>
#include <initializer_list>
#include <iterator>
#include <stack>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/graph/detail/adjacency_list.hpp>
#include <boost/graph/detail/edge.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/property_map/property_map.hpp>
#include <clang/AST/DeclCXX.h>
#include <llvm/Support/Casting.h>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/functional/comparisons.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"
#include "get_me/type_set.hpp"

[[nodiscard]] static bool permutationExists(const std::vector<PathType> &Paths,
                                            const PathType &CurrentPath,
                                            const GraphType &Graph,
                                            const GraphData &Data) {
  const auto IndexMap = get(boost::edge_index, Graph);
  const auto ToEdgeWeight = [&IndexMap, &Data](const EdgeDescriptor &Edge) {
    return Data.EdgeWeights[get(IndexMap, Edge)];
  };
  return ranges::any_of(Paths, [&ToEdgeWeight,
                                &CurrentPath](const PathType &ExistingPath) {
    return ranges::is_permutation(CurrentPath, ExistingPath, ranges::equal_to{},
                                  ToEdgeWeight, ToEdgeWeight);
  });
}

[[nodiscard]] static auto createIsValidPathPredicate(const Config &Conf) {
  return [&](const PathType &CurrentPath) {
    return Conf.MaxPathLength >= CurrentPath.size();
  };
}

[[nodiscard]] static auto
createContinuePathSearchPredicate(const Config &Conf,
                                  const ranges::range auto &CurrentPaths) {
  return [&]() { return Conf.MaxPathCount > CurrentPaths.size(); };
}

// FIXME: there exist paths that contain edges with aliased types and edges with
// their base types, basically creating redundant/non-optimal paths
// FIXME: don't produce paths that end up with the queried type
std::vector<PathType> pathTraversal(const GraphType &Graph,
                                    const GraphData &Data, const Config &Conf,
                                    const VertexDescriptor SourceVertex) {
  using possible_path_type = std::pair<VertexDescriptor, EdgeDescriptor>;

  PathType CurrentPath{};
  // FIXME: use a set with a comparator of permutations
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

  const auto AddToStackFactory = [&EdgesStack](auto Vertex) {
    return [&EdgesStack, Vertex](auto Val) {
      EdgesStack.emplace(Vertex, std::move(Val));
    };
  };

  ranges::for_each(toRange(out_edges(SourceVertex, Graph)),
                   AddToStackFactory(SourceVertex));

  const auto CurrentPathContainsTargetOfEdge = [&CurrentPath,
                                                &Graph](const auto &Edge) {
    const auto HasTargetEdge = [&Graph, TargetVertex = target(Edge, Graph)](
                                   const EdgeDescriptor &EdgeInPath) {
      return source(EdgeInPath, Graph) == TargetVertex ||
             target(EdgeInPath, Graph) == TargetVertex;
    };
    return !ranges::any_of(CurrentPath, HasTargetEdge);
  };
  const auto AddOutEdgesOfVertexToStack =
      [&Graph, &AddToStackFactory,
       &CurrentPathContainsTargetOfEdge](auto Vertex) {
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
  std::vector<PathType> Res{};

  for (const auto &Path : Paths) {
    if (const auto EquivalentPathContainedInResult =
            permutationExists(Res, Path, Graph, Data);
        EquivalentPathContainedInResult) {
      continue;
    }

    Res.push_back(Path);
  }

  return Res;
}

[[nodiscard]] static auto matchesNamePredicateFactory(std::string Name) {
  return [Name = std::move(Name)](const TypeSetValueType &Val) {
    return std::visit(
        Overloaded{
            [&](const clang::Type *const Type) {
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
            [](const ArithmeticType &) { return false; },
            [](const StdType &) { return false; }},
        Val);
  };
}

static void initializeVertexDataWithQueried(
    const std::vector<TransitionType> &TypeSetTransitionData, GraphData &Data,
    const std::string &TypeName) {
  const auto ToAcquired = [](const TransitionType &Val) {
    return std::get<0>(Val);
  };
  const auto matchesQueriedName = matchesNamePredicateFactory(TypeName);
  ranges::transform(ranges::views::filter(
                        TypeSetTransitionData,
                        [&Data, &matchesQueriedName](const TypeSet &Acquired) {
                          return ranges::any_of(Acquired, matchesQueriedName) &&
                                 !ranges::contains(Data.VertexData, Acquired);
                        },
                        ToAcquired),
                    std::back_inserter(Data.VertexData), ToAcquired);
}

template <typename T>
[[nodiscard]] static bool isSubset(const std::set<T> &Superset,
                                   const std::set<T> &Subset) {
  if (Subset.size() > Superset.size()) {
    return false;
  }
  auto SupersetIter = Superset.begin();
  const auto SupersetEnd = Superset.end();
  for (const auto &SubsetVal : Subset) {
    for (; SupersetIter != SupersetEnd && *SupersetIter != SubsetVal;
         ++SupersetIter) {
    }
  }
  return SupersetIter != SupersetEnd;
}

[[nodiscard]] static auto isSubsetPredicateFactory(const TypeSet &Subset) {
  return
      [&Subset](const TypeSet &Superset) { return isSubset(Superset, Subset); };
}

[[nodiscard]] static TypeSet merge(TypeSet Lhs, TypeSet Rhs) {
  Lhs.merge(Rhs);
  return Lhs;
}

[[nodiscard]] static TypeSet subtract(const TypeSet &Lhs, const TypeSet &Rhs) {
  TypeSet Res{};
  ranges::set_difference(Lhs, Rhs, std::inserter(Res, Res.end()), std::less{});
  return Res;
}

static void buildGraph(const std::vector<TransitionType> &TypeSetTransitionData,
                       GraphData &Data, const Config &Conf) {
  using indexed_vertex_type = std::pair<TypeSet, size_t>;
  std::set<indexed_vertex_type> VertexData =
      ranges::to<std::set>(ranges::views::zip(
          Data.VertexData, ranges::views::iota(static_cast<size_t>(0U))));

  std::vector<GraphData::EdgeType> EdgesData{};

  size_t IterationCount = 0U;
  auto TypeSetsOfInterest = VertexData;

  Data.VertexData.emplace_back();

  for (bool AddedTransitions = true;
       AddedTransitions && IterationCount < Conf.MaxGraphDepth;
       ++IterationCount) {
    std::set<indexed_vertex_type> TemporaryVertexData{};
    std::vector<GraphData::EdgeType> TemporaryEdgeData{};
    // FIXME: this needs to know the position of the TS in Data.VertexData
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

        const auto [NewRequiredTypeSetIndexExists, NewRequiredTypeSetIndex] =
            [&NewRequiredTypeSet, &TemporaryVertexData, &VertexData]() {
              const auto GetExistingOpt =
                  [&NewRequiredTypeSet](const auto &Container)
                  -> std::optional<std::pair<bool, size_t>> {
                if (const auto Iter =
                        ranges::find(Container, NewRequiredTypeSet,
                                     &indexed_vertex_type::first);
                    Iter != Container.end()) {
                  return std::pair{true, Iter->second};
                }
                return std::nullopt;
              };
              if (const auto Existing = GetExistingOpt(TemporaryVertexData);
                  Existing) {
                return Existing.value();
              }
              return GetExistingOpt(VertexData)
                  .value_or(std::pair{false, VertexData.size() +
                                                 TemporaryVertexData.size()});
            }();

        const auto EdgeToAdd =
            std::pair{SourceTypeSetIndex, NewRequiredTypeSetIndex};

        if (const auto TransitionToAddAlreadyExistsInContainer =
                [EdgeToAdd, &Transition,
                 &EdgeWeights =
                     Data.EdgeWeights]<typename T>(const T &Container) {
                  if (!ranges::contains(Container, EdgeToAdd)) {
                    return false;
                  }
                  return ranges::contains(EdgeWeights, Transition);
                };
            NewRequiredTypeSetIndexExists &&
            (TransitionToAddAlreadyExistsInContainer(TemporaryEdgeData) ||
             TransitionToAddAlreadyExistsInContainer(EdgesData))) {
          continue;
        }

        if (!NewRequiredTypeSetIndexExists) {
          TemporaryVertexData.emplace(std::move(NewRequiredTypeSet),
                                      NewRequiredTypeSetIndex);
        }

        TemporaryEdgeData.push_back(EdgeToAdd);
        Data.EdgeWeights.push_back(Transition);
        AddedTransitions = true;
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
    // FIXME: move this filling of TypeSetsOfInterest into loop to remove
    // redundant traversal of VertexData
    TypeSetsOfInterest = ranges::to<std::set>(
        VertexData |
        ranges::views::filter(
            [&TemporaryEdgeData](const auto &Val) {
              return ranges::contains(TemporaryEdgeData, Val,
                                      &GraphData::EdgeType::second);
            },
            &indexed_vertex_type::second));

    EdgesData.insert(EdgesData.end(),
                     std::make_move_iterator(TemporaryEdgeData.begin()),
                     std::make_move_iterator(TemporaryEdgeData.end()));
    TemporaryEdgeData.clear();
  }
  spdlog::trace("{:=^50}", "");

  auto VertexDataToSort = ranges::to_vector(VertexData);
  ranges::sort(VertexDataToSort, std::less{}, &indexed_vertex_type::second);
  Data.VertexData = ranges::to_vector(
      ranges::views::transform(VertexDataToSort, &indexed_vertex_type::first));

  Data.Edges = ranges::to_vector(EdgesData);
  Data.EdgeIndices = ranges::to_vector(
      ranges::views::iota(static_cast<size_t>(0U), Data.Edges.size()));
}

std::pair<GraphType, GraphData>
createGraph(const std::vector<TransitionType> &TypeSetTransitionData,
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

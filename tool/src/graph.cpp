#include "get_me/graph.hpp"

#include <functional>
#include <iterator>
#include <stack>

#include <boost/graph/detail/adjacency_list.hpp>
#include <boost/graph/detail/edge.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/property_map/property_map.hpp>
#include <clang/AST/DeclCXX.h>
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
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"
#include "get_me/tooling.hpp"
#include "get_me/utility.hpp"

// FIXME: don't produce paths that end up with the queried type
std::vector<PathType> pathTraversal(const GraphType &Graph,
                                    const VertexDescriptor SourceVertex) {
  using ranges::contains;
  using ranges::find;
  using ranges::for_each;
  using possible_path_type = std::pair<
      VertexDescriptor,
      typename boost::graph_traits<GraphType>::out_edge_iterator::value_type>;

  PathType CurrentPath{};
  std::vector<PathType> Paths{};
  std::stack<possible_path_type> EdgesStack{};

  const auto AddOutEdgesOfVertexToStack = [&Graph, &EdgesStack, &Paths,
                                           &CurrentPath](auto Vertex) {
    const auto Vec = ranges::to_vector(toRange(out_edges(Vertex, Graph)));
    auto OutEdgesRange =
        ranges::views::filter(Vec, [&CurrentPath](const auto &Edge) {
          return !ranges::contains(CurrentPath, Edge);
        });
    if (OutEdgesRange.empty()) {
      return false;
    }
    spdlog::trace(
        "path #{}: adding edges {} to EdgesStack", Paths.size(),
        OutEdgesRange |
            ranges::views::transform(
                [&Graph](const typename boost::graph_traits<
                         GraphType>::out_edge_iterator::value_type &Val) {
                  return std::pair{source(Val, Graph), target(Val, Graph)};
                }));

    const auto AddToStack = [&EdgesStack, Vertex](auto Val) {
      EdgesStack.emplace(Vertex, std::move(Val));
    };
    for_each(std::move(OutEdgesRange), AddToStack);

    return true;
  };

  AddOutEdgesOfVertexToStack(SourceVertex);

  VertexDescriptor CurrentVertex{};
  VertexDescriptor PrevTarget{SourceVertex};
  while (!EdgesStack.empty()) {
    auto PathIndex = Paths.size();
    const auto [Src, Edge] = EdgesStack.top();
    EdgesStack.pop();

    if (contains(CurrentPath, Edge)) {
      spdlog::error("skipping visiting edge already in path");
      continue;
    }

    spdlog::trace(
        "path #{}: src: {}, prev target: {}, edge: {}, current path: {}",
        PathIndex, Src, PrevTarget, Edge, CurrentPath);

    if (!CurrentPath.empty() && target(CurrentPath.back(), Graph) != Src) {
      // visiting an edge whose source is not the target of the previous edge.
      // the current path has to be reversed until the new edge can be added to
      // the path
      // remove edges that were added after the path got to src
      const auto Msg =
          fmt::format("path #{}: reverting current path {} back to ", PathIndex,
                      CurrentPath);
      const auto GetEdgeSource = [&Graph](const EdgeDescriptor &Val) {
        return source(Val, Graph);
      };
      CurrentPath.erase(find(CurrentPath, Src, GetEdgeSource),
                        CurrentPath.end());
      spdlog::trace("{}{}", Msg, CurrentPath);
      PrevTarget = Src;
    } else {
      PrevTarget = target(Edge, Graph);
    }

    CurrentPath.emplace_back(Edge);
    CurrentVertex = target(Edge, Graph);
    if (const auto IsFinalVertexInPath =
            !AddOutEdgesOfVertexToStack(CurrentVertex);
        IsFinalVertexInPath) {
      Paths.push_back(CurrentPath);
    }

    spdlog::trace("path #{}: post algo src: {}, prev target: {}, edge: {}, "
                  "current path: {}",
                  PathIndex, Src, PrevTarget, Edge, CurrentPath);
  }

  return Paths;
}

std::vector<PathType> independentPaths(const std::vector<PathType> &Paths,
                                       const GraphType &Graph) {
  using ranges::any_of;
  using ranges::is_permutation;
  using ranges::views::transform;

  std::vector<PathType> Res{};

  const auto Weightmap = get(boost::edge_weight, Graph);
  const auto ToWeights = [&Weightmap](const EdgeDescriptor &Edge) {
    return get(Weightmap, Edge);
  };

  for (const auto &Path : Paths) {
    if (const auto EquivalentPathContainedInResult =
            any_of(Res,
                   [&Path, &ToWeights](const auto &PathInRes) {
                     return is_permutation(Path | transform(ToWeights),
                                           PathInRes | transform(ToWeights));
                   });
        EquivalentPathContainedInResult) {
      continue;
    }

    Res.push_back(Path);
  }

  return Res;
}

[[nodiscard]] static auto matchesName(std::string Name) {
  return [Name = std::move(Name)](const TypeSetValueType &Val) {
    const auto QType = clang::QualType(Val.Value, 0);
    const auto TypeAsString = QType.getAsString();
    const auto TypeAsStringRef = [&]() {
      if (auto *Rec = QType->getAsRecordDecl()) {
        return Rec->getName();
      }
      return llvm::StringRef(TypeAsString);
    }();
    const auto Res = TypeAsStringRef == llvm::StringRef{Name};
    if (!Res && TypeAsStringRef.contains(Name)) {
      spdlog::trace("matchesName(QualType): no match for close match: {} vs {}",
                    TypeAsStringRef, Name);
    }
    return Res;
  };
}

static void addQueriedTypeSetsAndAddEdgeWeights(
    const std::vector<TypeSetTransitionDataType> &TypeSetTransitionData,
    GraphData &Data, const std::string &TypeName) {
  for (const auto &[Acquired, Transition, Required] : TypeSetTransitionData) {
    if (ranges::any_of(Acquired, matchesName(TypeName)) &&
        !ranges::contains(Data.VertexData, Acquired)) {
      Data.VertexData.push_back(Acquired);
    }
  }
}

template <typename RangeType1, typename RangeType2>
[[nodiscard]] static bool isSubset(RangeType1 &&Range1, RangeType2 &&Range2) {
  return ranges::all_of(
      std::forward<RangeType2>(Range2),
      [Range = std::forward<RangeType1>(Range1)](const auto &Val) {
        return ranges::contains(Range, Val);
      });
}

[[nodiscard]] static auto
containsAcquiredTypeSet(const TypeSet &AcquiredTypeSet) {
  return [&AcquiredTypeSet](const TypeSet &Val) {
    return isSubset(Val | ranges::views::transform(&TypeSetValueType::Value),
                    AcquiredTypeSet |
                        ranges::views::transform(&TypeSetValueType::Value));
  };
}

[[nodiscard]] static TypeSet mergeTypeSets(TypeSet Lhs, TypeSet Rhs) {
  Lhs.merge(Rhs);
  return Lhs;
}

[[nodiscard]] static TypeSet subtractTypeSets(const TypeSet &Lhs,
                                              const TypeSet &Rhs) {
  TypeSet Res{};
  ranges::set_difference(Lhs, Rhs, std::inserter(Res, Res.end()), std::less{});
  return Res;
}

static void
buildGraph(const std::vector<TypeSetTransitionDataType> &TypeSetTransitionData,
           GraphData &Data) {
  using indexed_vertex_type = std::pair<TypeSet, size_t>;
  std::set<indexed_vertex_type> VertexData =
      ranges::to<std::set>(ranges::views::zip(
          Data.VertexData, ranges::views::iota(static_cast<size_t>(0U))));

  std::set<GraphData::EdgeType> EdgesData{};

  size_t IterationCount = 0U;
  auto TypeSetsOfInterest = VertexData;
  for (bool AddedTransitions = true; AddedTransitions; ++IterationCount) {
    std::set<GraphData::EdgeType> TemporaryEdgeData{};
    std::set<indexed_vertex_type> TemporaryVertexData{};
    // FIXME: this needs to know the position of the TS in Data.VertexData
    AddedTransitions = false;
    size_t TransitionCounter = 0U;
    spdlog::trace("{:=^50}", "");
    const auto TransitionWithInterestingAcquiredTypeSet =
        [&TypeSetsOfInterest](const TypeSetTransitionDataType &Val) {
          const auto &Acquired = std::get<0>(Val);
          return ranges::any_of(TypeSetsOfInterest,
                                containsAcquiredTypeSet(Acquired),
                                &indexed_vertex_type::first);
        };
    spdlog::trace("TypeSetsOfInterest: {}", TypeSetsOfInterest);
    for (const auto FilteredTypeSetTransitionData = ranges::to_vector(
             TypeSetTransitionData |
             ranges::views::filter(TransitionWithInterestingAcquiredTypeSet));
         const auto &[AcquiredTypeSet, Transition, RequiredTypeSet] :
         FilteredTypeSetTransitionData) {
      ++TransitionCounter;
      for (const auto FilteredTypeSetsOfInterest = ranges::to_vector(
               TypeSetsOfInterest |
               ranges::views::filter(containsAcquiredTypeSet(AcquiredTypeSet),
                                     &indexed_vertex_type::first));
           const auto &[VertexTypeSet, SourceTypeSetIndex] :
           FilteredTypeSetsOfInterest) {
        auto NewRequiredTypeSet = mergeTypeSets(
            subtractTypeSets(VertexTypeSet, AcquiredTypeSet), RequiredTypeSet);

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

        if (const auto EdgeToAddAlreadyExistsInContainer =
                [EdgeToAdd]<typename T>(const T &Container) {
                  return Container.contains(EdgeToAdd);
                };
            NewRequiredTypeSetIndexExists &&
            (EdgeToAddAlreadyExistsInContainer(TemporaryEdgeData) ||
             EdgeToAddAlreadyExistsInContainer(EdgesData))) {
          spdlog::trace("edge to add already exists: {}", EdgeToAdd);
          continue;
        }

        if (!NewRequiredTypeSetIndexExists) {
          spdlog::trace("adding new type set: #{}({}), not included in {}",
                        NewRequiredTypeSetIndex, NewRequiredTypeSet,
                        VertexData);
          TemporaryVertexData.emplace(std::move(NewRequiredTypeSet),
                                      NewRequiredTypeSetIndex);
        }

        TemporaryEdgeData.insert(EdgeToAdd);
        Data.EdgeWeightMap.try_emplace(EdgeToAdd, Transition);
        Data.EdgeWeights.push_back(Transition);

        AddedTransitions = true;
      }
      spdlog::trace("#{} transition #{} (|V| = {}(+{}), |E| = {}(+{})): {}",
                   IterationCount, TransitionCounter,
                   VertexData.size() + TemporaryVertexData.size(),
                   TemporaryVertexData.size(),
                   EdgesData.size() + TemporaryEdgeData.size(),
                   TemporaryEdgeData.size(), Transition);
      spdlog::trace("TemporaryVertexData: {}", TemporaryVertexData);
      spdlog::trace("TemporaryEdgeData: {}", TemporaryEdgeData);
    }

    VertexData.merge(std::move(TemporaryVertexData));
    // FIXME: move this filling of TypeSetsOfInterest into loop to remove
    // redundant traversal of VertexData
    TypeSetsOfInterest = ranges::to<std::set>(
        VertexData |
        ranges::views::filter(
                         [&TemporaryEdgeData](const auto &Val) {
              return ranges::binary_search(TemporaryEdgeData, Val, std::less{},
                               &GraphData::EdgeType::second);
                         },
                         &indexed_vertex_type::second));
    TemporaryVertexData.clear();

    EdgesData.merge(std::move(TemporaryEdgeData));
    TemporaryEdgeData.clear();
  }
  spdlog::trace("{:=^50}", "");

  auto VertexDataToSort = ranges::to_vector(VertexData);
  ranges::sort(VertexDataToSort, std::less{}, &indexed_vertex_type::second);
  Data.VertexData = ranges::to_vector(
      ranges::views::transform(VertexDataToSort, &indexed_vertex_type::first));

  Data.Edges = ranges::to_vector(EdgesData);
}

std::pair<GraphType, GraphData>
createGraph(const std::vector<TypeSetTransitionDataType> &TypeSetTransitionData,
    const std::string &TypeName) {
  GraphData Data{};
  addQueriedTypeSetsAndAddEdgeWeights(TypeSetTransitionData, Data, TypeName);

  if (!ranges::contains(Data.VertexData, TypeSet{})) {
    Data.VertexData.emplace_back();
  }

  spdlog::trace("initial GraphData.VertexData: {}", Data.VertexData);

  buildGraph(TypeSetTransitionData, Data);

  spdlog::trace("GraphData.VertexData: {}", Data.VertexData);
  spdlog::trace("GraphData.Edges: {}", Data.Edges);
  spdlog::trace("GraphData.EdgeWeights: {}", Data.EdgeWeights);
  spdlog::trace("GraphData.EdgeWeightMap: {}", Data.EdgeWeightMap);

  return {GraphType(Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
                    Data.EdgeWeights.data(), Data.EdgeWeights.size()),
          Data};
}

[[nodiscard]] static std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FunctionDecl *FDecl) {
  const auto AcquiredType = [FDecl]() {
    if (const auto *const Constructor =
            llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl);
        Constructor) {
      const auto *const Decl = Constructor->getParent();
      return TypeSetValueType{Decl->getTypeForDecl()};
    }
    const auto RQType = FDecl->getReturnType().getCanonicalType();
    const auto *const ReturnTypePtr = RQType.getTypePtr();
    return TypeSetValueType{ReturnTypePtr};
  }();
  const auto RequiredTypes = [FDecl]() {
    const auto Parameters = FDecl->parameters();
    auto ParameterTypeRange =
        Parameters |
        ranges::views::transform([](const clang::ParmVarDecl *PVDecl) {
          const auto QType = PVDecl->getType().getCanonicalType();
          return TypeSetValueType{QType.getTypePtr()};
        });
    auto Res = TypeSet{std::make_move_iterator(ParameterTypeRange.begin()),
                       std::make_move_iterator(ParameterTypeRange.end())};
    if (const auto *const Method =
            llvm::dyn_cast<clang::CXXMethodDecl>(FDecl)) {
      if (!llvm::isa<clang::CXXConstructorDecl>(Method) &&
          !Method->isStatic()) {
        Res.emplace(Method->getParent()->getTypeForDecl());
      }
    }
    return Res;
  }();
  return {{AcquiredType}, RequiredTypes};
}

[[nodiscard]] static std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FieldDecl *FDecl) {
  return {{{FDecl->getType().getCanonicalType().getTypePtr()}},
          {{FDecl->getParent()->getTypeForDecl()}}};
}

std::vector<TypeSetTransitionDataType>
getTypeSetTransitionData(const TransitionCollector &Collector) {
  return ranges::to_vector(ranges::views::transform(
      Collector.Data, [](const TransitionDataType &Val) {
        return std::visit(
            Overloaded{
                [&Val](const auto *Decl) -> TypeSetTransitionDataType {
                  auto [Acquired, Required] = toTypeSet(Decl);
                  return {{std::move(Acquired)}, Val, std::move(Required)};
                },
                [](const std::monostate) -> TypeSetTransitionDataType {
                  return {};
                }},
            Val);
      }));
}

VertexDescriptor
getSourceVertexMatchingQueriedType(const GraphData &Data,
                                   const std::string &QueriedType) {
  // FIXME: improve queried type matching:
  // - better matching of names
  // - allow matching mutiple to get around QualType vs NamedDecl problem
  // - better: fix QualType vs NamedDecl problem
  // FIXME: only getting the 'A' type, not the & qualified
  const auto SourceVertex =
      ranges::find_if(Data.VertexData, [&QueriedType](const TypeSet &TSet) {
        return TSet.end() != ranges::find_if(TSet, matchesName(QueriedType));
      });

  if (SourceVertex == Data.VertexData.end()) {
    spdlog::error("found no type matching {}", QueriedType);
    return 0;
  }
  return static_cast<VertexDescriptor>(
      std::distance(Data.VertexData.begin(), SourceVertex));
}

#include "get_me/graph.hpp"

#include <functional>
#include <stack>

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
    spdlog::info(
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

    spdlog::info(
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
      spdlog::info("{}{}", Msg, CurrentPath);
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

    spdlog::info("path #{}: post algo src: {}, prev target: {}, edge: {}, "
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
      spdlog::info("matchesName(QualType): no match for close match: {} vs {}",
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
    Data.EdgeWeights.push_back(Transition);
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
  ranges::set_difference(Lhs, Rhs, std::inserter(Res, Res.end()),
                         std::less<>{});
  return Res;
}

static void buildVertices(
    const std::vector<TypeSetTransitionDataType> &TypeSetTransitionData,
    GraphData &Data) {
  // FIXME: build graph only for the required nodes
  // start with building all edges from the source/queried type to others
  // then add these vertices to the working list
  size_t IterationCount = 0U;
  for (bool AddedTransitions = true; AddedTransitions; ++IterationCount) {
    std::vector<TypeSet> TemporaryVertexData{};
    std::vector<TypeSet> TypeSetsOfInterest = Data.VertexData;
    AddedTransitions = false;
    size_t TransitionCounter = 0U;
    spdlog::info("{:=^30}", "");
    for (const auto &[AcquiredTypeSet, Transition, RequiredTypeSet] :
         TypeSetTransitionData |
             ranges::views::filter(
                 [&TypeSetsOfInterest](const TypeSetTransitionDataType &Val) {
                   const auto &Acquired = std::get<0>(Val);
                   const auto Res = ranges::any_of(
                       TypeSetsOfInterest, containsAcquiredTypeSet(Acquired));
                   return Res;
                 })) {
      ++TransitionCounter;
      for (const auto &VertexTypeSet :
           ranges::to_vector(TypeSetsOfInterest |
                             ranges::views::filter(
                                 containsAcquiredTypeSet(AcquiredTypeSet)))) {
        if (auto NewRequiredTypeSet =
                mergeTypeSets(subtractTypeSets(VertexTypeSet, AcquiredTypeSet),
                              RequiredTypeSet);
            !ranges::contains(TemporaryVertexData, NewRequiredTypeSet) &&
            !ranges::contains(Data.VertexData, NewRequiredTypeSet)) {
          TemporaryVertexData.push_back(std::move(NewRequiredTypeSet));
          AddedTransitions = true;
        }
      }
      Data.VertexData.insert(Data.VertexData.end(), TemporaryVertexData.begin(),
                             TemporaryVertexData.end());
      TypeSetsOfInterest = std::move(TemporaryVertexData);
      TemporaryVertexData.clear();

      spdlog::info("#{} added transition ({}/{}) (total vertices: {}): {}, "
                   "VertexData: {}",
                   IterationCount, TransitionCounter,
                   TypeSetTransitionData.size(), Data.VertexData.size(),
                   Transition, Data.VertexData);
    }
  }
  spdlog::info("{:=^30}", "");
}

static void
buildEdges(const std::vector<TypeSetTransitionDataType> &TypeSetTransitionData,
           GraphData &Data) {
  // FIXME: the types for C are different (currently 3 and 6), this might be
  // because one type is from getting the parent of a record member and the
  // other is a return type/parameter type of a function
  for (const auto &Transition : TypeSetTransitionData) {
    const auto &AcquiredTypeSet = std::get<0>(Transition);
    // FIXME: rename to Transition
    const auto &Function = std::get<1>(Transition);
    if (std::visit(
            Overloaded{[](const clang::FunctionDecl *FDecl) {
                         return llvm::isa<clang::CXXConversionDecl>(FDecl) ||
                                llvm::isa<clang::CXXDestructorDecl>(FDecl);
                       },
                       [](auto) { return false; }},
            Function)) {
      continue;
    }
    const auto &RequiredTypeSet = std::get<2>(Transition);

    ranges::range auto Acquired = ranges::views::filter(
        ranges::views::zip(Data.VertexData, ranges::views::iota(0U)),
        [&AcquiredTypeSet](const auto &EnumeratedTSet) {
          return ranges::includes(EnumeratedTSet.first, AcquiredTypeSet);
        });

    for (auto AcquiredValueIndexPair : Acquired) {
      const auto TargetTypeSet = mergeTypeSets(
          subtractTypeSets(AcquiredValueIndexPair.first, AcquiredTypeSet),
          RequiredTypeSet);
      const auto TargetVertexIndex =
          std::distance(Data.VertexData.begin(),
                        ranges::find(Data.VertexData, TargetTypeSet));
      // FIXME: how do duplicate edges get inserted without this check?
      if (ranges::contains(Data.Edges,
                           GraphData::EdgeType{AcquiredValueIndexPair.second,
                                               TargetVertexIndex})) {
        continue;
      }
      Data.Edges.emplace_back(AcquiredValueIndexPair.second, TargetVertexIndex);
      Data.EdgeWeightMap.try_emplace(
          {AcquiredValueIndexPair.second, TargetVertexIndex}, Function);
    }
  }
}

GraphData generateVertexAndEdgeWeigths(
    const std::vector<TypeSetTransitionDataType> &TypeSetTransitionData,
    std::string TypeName) {
  GraphData Data{};
  addQueriedTypeSetsAndAddEdgeWeights(TypeSetTransitionData, Data, TypeName);

  if (!ranges::contains(Data.VertexData, TypeSet{})) {
    Data.VertexData.emplace_back();
  }

  spdlog::info("initial GraphData.VertexData: {}", Data.VertexData);

  buildVertices(TypeSetTransitionData, Data);

  spdlog::info("GraphData.VertexData: {}", Data.VertexData);

  buildEdges(TypeSetTransitionData, Data);

  spdlog::info("GraphData.Edges: {}", Data.Edges);
  spdlog::info("GraphData.EdgeWeights: {}", Data.EdgeWeights);
  spdlog::info("GraphData.EdgeWeightMap: {}", Data.EdgeWeightMap);

  return Data;
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

// FIXME: skip this step and do directly TransitionCollector -> GraphData
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
getSourceVertexMatchingQueriedType(GraphData &Data,
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

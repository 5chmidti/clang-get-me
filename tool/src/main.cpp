#include <cstddef>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <type_traits>
#include <variant>

#include <bits/ranges_base.h>
#include <boost/algorithm/string/predicate.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/PrecompiledPreamble.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/iterator/operations.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/range/traits.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/counted.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/zip.hpp>

#include "get_me/formatting.hpp"
#include "get_me/graph.hpp"
#include "get_me/tooling.hpp"
#include "get_me/utility.hpp"

// NOLINTBEGIN
using namespace ranges;
using namespace llvm::cl;

static OptionCategory ToolCategory("get_me");
static opt<std::string> TypeName("t", desc("Name of the type to get"), Required,
                                 ValueRequired, cat(ToolCategory));
// NOLINTEND

using TypeSetTransitionDataType =
    std::tuple<TypeSet, TransitionDataType, TypeSet>;

[[nodiscard]] static std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FunctionDecl *FDecl) {
  const auto AcquiredType = [FDecl]() -> TypeSetValueType {
    if (const auto *const CDecl =
            llvm::dyn_cast<clang::CXXConstructorDecl>(FDecl);
        CDecl) {
      const auto *const Decl = CDecl->getParent();
      return {Decl->getTypeForDecl(), Decl};
    }
    const auto RQType = FDecl->getReturnType();
    return {RQType.getTypePtr(), RQType};
  }();
  const auto RequiredTypes = [FDecl]() {
    const auto Parameters = FDecl->parameters();
    auto ParameterTypeRange =
        Parameters |
        views::transform(
            [](const clang::ParmVarDecl *PVDecl) -> TypeSetValueType {
              const auto QType = PVDecl->getType();
              return {QType.getTypePtr(), QType};
            });
    auto Res = TypeSet{std::make_move_iterator(ParameterTypeRange.begin()),
                       std::make_move_iterator(ParameterTypeRange.end())};
    if (const auto *const Method =
            llvm::dyn_cast<clang::CXXMethodDecl>(FDecl)) {
      const auto *const RDecl = Method->getParent();
      auto Val = TypeSetValueType{RDecl->getTypeForDecl(), RDecl};
      spdlog::info("adding record: {}", Val);
      Res.emplace(Val);
    }
    return Res;
  }();
  return {{AcquiredType}, RequiredTypes};
}

[[nodiscard]] static std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FieldDecl *FDecl) {
  const auto FQType = FDecl->getType();
  const auto *const RDecl = FDecl->getParent();
  return {{{FQType.getTypePtr(), FQType}}, {{RDecl->getTypeForDecl(), RDecl}}};
}

// FIXME: skip this step and do directly TransitionCollector -> GraphData
[[nodiscard]] static std::vector<TypeSetTransitionDataType>
getTypeSetTransitionData(const TransitionCollector &Collector) {
  return to_vector(
      views::transform(Collector.Data, [](const TransitionDataType &Val) {
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

[[nodiscard]] static TypeSet mergeTypeSets(TypeSet Lhs, TypeSet Rhs) {
  Lhs.merge(Rhs);
  return Lhs;
}

[[nodiscard]] static TypeSet subtractTypeSets(const TypeSet &Lhs,
                                              const TypeSet &Rhs) {
  TypeSet Res{};
  set_difference(Lhs, Rhs, std::inserter(Res, Res.end()), std::less<>{});
  return Res;
}

template <typename RangeType1, typename RangeType2>
[[nodiscard]] static bool isSubset(RangeType1 &&Range1, RangeType2 &&Range2) {
  return all_of(std::forward<RangeType2>(Range2),
                [Range = std::forward<RangeType1>(Range1)](const auto &Val) {
                  return contains(Range, Val);
                });
}

[[nodiscard]] static auto
containsAcquiredTypeSet(const TypeSet &AcquiredTypeSet) {
  return [&AcquiredTypeSet](const TypeSet &Val) {
    return isSubset(Val | views::transform(&TypeSetValueType::Value),
                    AcquiredTypeSet |
                        views::transform(&TypeSetValueType::Value));
  };
}

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  auto OptionsParser =
      clang::tooling::CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!OptionsParser) {
    llvm::errs() << OptionsParser.takeError();
    return 1;
  }
  const auto &Sources = OptionsParser->getSourcePathList();
  clang::tooling::ClangTool Tool(
      OptionsParser->getCompilations(),
      Sources.empty() ? OptionsParser->getCompilations().getAllFiles()
                      : Sources);

  GraphData Data{};

  TransitionCollector Collector{};
  std::vector<std::unique_ptr<clang::ASTUnit>> ASTs{};
  if (const auto BuildASTsResult = Tool.buildASTs(ASTs); BuildASTsResult != 0) {
    spdlog::error("error building ASTs");
    return 1;
  }
  auto Consumer = GetMe{Collector};
  for (const auto &AST : ASTs) {
    Consumer.HandleTranslationUnit(AST->getASTContext());
  }

  fmt::print("{}\n", Collector.Data);
  // build graph from gathered graph data
  const auto TypeSetTransitionData = getTypeSetTransitionData(Collector);
  fmt::print("TypeSetTransitionData: {}\n", TypeSetTransitionData);

  for (const auto &[Acquired, Transition, Required] : TypeSetTransitionData) {
    if (const auto Iter = find_if(
            Acquired,
            [](const TypeSetValueType &Val) {
              const auto Name = TypeName.getValue();
              const auto NameWithoutNull =
                  std::span{Name.begin(), Name.end() - 1};
              if (Val.MetaValue.valueless_by_exception()) {
                spdlog::error("Val.MetaValue is valueless_by_exception");
                return false;
              }
              return std::visit(
                  Overloaded{[NameWithoutNull](const clang::NamedDecl *NDecl) {
                               const auto NameOfDecl = NDecl->getName();
                               const auto Res = NameOfDecl.contains(
                                   llvm::StringRef{NameWithoutNull.data(),
                                                   NameWithoutNull.size()});
                               return Res;
                             },
                             [&Name](const clang::QualType &QType) {
                               QType->isIntegerType();
                               const auto TypeAsString = QType.getAsString();
                               const auto Res =
                                   boost::contains(TypeAsString, Name);
                               return Res;
                             },
                             [](std::monostate) { return false; }},
                  Val.MetaValue);
            });
        Iter != Acquired.end() && !contains(Data.VertexData, Acquired)) {
      Data.VertexData.push_back(Acquired);
    }
  }

  transform(TypeSetTransitionData, std::back_inserter(Data.EdgeWeights),
            [](const auto &Transition) { return std::get<1>(Transition); });

  for (size_t NumAddedTransitions = std::numeric_limits<size_t>::max();
       NumAddedTransitions != 0;) {
    std::vector<TypeSet> TemporaryVertexData{};
    NumAddedTransitions = 0U;
    fmt::print("=======================\n");
    for (const auto &[AcquiredTypeSet, Transition, RequiredTypeSet] :
         TypeSetTransitionData) {
      for (const auto &VertexTypeSet :
           to_vector(Data.VertexData |
                     views::filter(containsAcquiredTypeSet(AcquiredTypeSet)))) {
        auto NewRequiredTypeSet = mergeTypeSets(
            subtractTypeSets(VertexTypeSet, AcquiredTypeSet), RequiredTypeSet);
        if (const auto Iter = find(Data.VertexData, NewRequiredTypeSet);
            Iter == Data.VertexData.end()) {
          TemporaryVertexData.push_back(std::move(NewRequiredTypeSet));
          fmt::print("added transition: {}\n", Transition);
          ++NumAddedTransitions;
        }
      }
      Data.VertexData.insert(
          Data.VertexData.end(),
          std::make_move_iterator(TemporaryVertexData.begin()),
          std::make_move_iterator(TemporaryVertexData.end()));
      TemporaryVertexData.clear();
    }
  }

  fmt::print("GraphData.VertexData: {}\n", Data.VertexData);

  // FIXME: only build graph for the queried type
  // FIXME: check if this takes into account partially required type sets
  // would not directly speed up the algorithm, but would save memory
  // FIXME: the types for C are different (currently 3 and 6), this might be
  // because one type is from getting the parent of a record member and the
  // other is a return type/parameter type of a function
  for (const auto &Transition : TypeSetTransitionData) {
    const auto &AcquiredTypeSet = std::get<0>(Transition);
    // FIXME rename to Transition
    const auto &Function = std::get<1>(Transition);
    if (std::visit(
            Overloaded{[](const clang::FunctionDecl *FDecl) {
                         return llvm::isa<clang::CXXConversionDecl>(FDecl) ||
                                llvm::isa<clang::CXXConstructorDecl>(FDecl) ||
                                llvm::isa<clang::CXXDestructorDecl>(FDecl);
                       },
                       [](auto) { return false; }},
            Function)) {
      continue;
    }
    const auto &RequiredTypeSet = std::get<2>(Transition);
    constexpr auto ToIndex = [](const auto &Pair) { return Pair.second; };

    range auto Acquired = views::transform(
        views::filter(views::zip(Data.VertexData, views::iota(0U)),
                      [&AcquiredTypeSet](const auto &EnumeratedTSet) {
                        return includes(EnumeratedTSet.first, AcquiredTypeSet);
                      }),
        ToIndex);
    const auto RequiredTypeSetEmpty = RequiredTypeSet.empty();
    const auto RequiredComparator =
        [&RequiredTypeSet, RequiredTypeSetEmpty](const auto &EnumeratedTSet) {
          if (RequiredTypeSetEmpty) {
            return ranges::empty(EnumeratedTSet.first);
          }
          return includes(EnumeratedTSet.first, RequiredTypeSet);
        };
    range auto Required = views::transform(
        views::filter(views::zip(Data.VertexData, views::iota(0U)),
                      RequiredComparator),
        ToIndex);
    spdlog::info("Transition: {}", Transition);
    spdlog::info("Acquired: {}", Acquired);
    spdlog::info("Required: {}", Required);
    for (const auto [AcquiredIndex, RequiredIndex] :
         views::cartesian_product(Acquired, Required)) {
      // FIXME: how do duplicate edges get inserted without this check?
      if (contains(Data.Edges,
                   GraphData::EdgeType{AcquiredIndex, RequiredIndex})) {
        continue;
      }
      spdlog::info("adding: ({}, {}) i.e. {}", AcquiredIndex, RequiredIndex,
                   Transition);
      Data.Edges.emplace_back(AcquiredIndex, RequiredIndex);
      Data.EdgeWeightMap.try_emplace({AcquiredIndex, RequiredIndex}, Function);
    }
  }

  // FIXME: apply greedy transition traversal strategy

  fmt::print("GraphData.Edges: {}\n", Data.Edges);
  fmt::print("GraphData.EdgeWeights: {}\n", Data.EdgeWeights);
  fmt::print("GraphData.EdgeWeightMap: {}\n", Data.EdgeWeightMap);

  GraphType Graph(Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
                  Data.EdgeWeights.data(), Data.EdgeWeights.size());

  const auto &QueriedType = TypeName.getValue();

  // FIXME: improve queried type matching:
  // - better matching of names
  // - allow matching mutiple to get around QualType vs NamedDecl problem
  // - better: fix QualType vs NamedDecl problem
  // FIXME: only getting the 'A' type, not the & qualified
  const auto SourceVertex = find_if(Data.VertexData, [&QueriedType](
                                                         const TypeSet &TSet) {
    return TSet.end() !=
           find_if(
               TSet,
               [&QueriedType](
                   const typename TypeSetValueType::meta_type &MetaVal) {
                 return std::visit(
                     Overloaded{[&QueriedType](clang::QualType QType) {
                                  return includes(QType.getAsString(),
                                                  QueriedType);
                                },
                                [&QueriedType](const clang::NamedDecl *NDecl) {
                                  return NDecl->getName().contains(QueriedType);
                                },
                                [](std::monostate) { return false; }},
                     MetaVal);
               },
               &TypeSetValueType::MetaValue);
  });

  if (SourceVertex == Data.VertexData.end()) {
    spdlog::error("found no type matching {}", QueriedType);
    return 1;
  }
  const auto SourceVertexDesc =
      static_cast<size_t>(std::distance(Data.VertexData.begin(), SourceVertex));

  const auto Paths = pathTraversal(Graph, SourceVertexDesc);

  for (const auto [Path, Number] : views::zip(Paths, views::iota(0U))) {
    spdlog::info(
        "path #{}: {}", Number,
        fmt::join(
            Path | views::transform([&Graph, &Data](const auto &Transition) {
              const auto Edge = std::pair{source(Transition, Graph),
                                          target(Transition, Graph)};
              const auto Weight = Data.EdgeWeightMap.at(Edge);
              const auto TargetName = getTransitionTargetTypeName(Weight);
              const auto SourceName = getTransitionSourceTypeName(Weight);
              return Weight;
            }),
            ", "));
  }

  std::ofstream DotFile("graph.dot");
  std::string Res{};
  Res += fmt::format("digraph D {{\n");

  for (const auto &Edge : toRange(edges(Graph))) {
    const auto SourceNode = source(Edge, Graph);
    const auto TargetNode = target(Edge, Graph);
    const auto EdgeWeight = Data.EdgeWeightMap.at({SourceNode, TargetNode});
    const auto TargetName = getTransitionTargetTypeName(EdgeWeight);
    const auto SourceName = getTransitionSourceTypeName(EdgeWeight);
    Res += fmt::format(
        R"(  "{}" -> "{}"[label="{}"]
)",
        SourceName, TargetName, EdgeWeight);
  }

  Res += fmt::format("}}");
  DotFile << Res;
}

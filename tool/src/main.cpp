#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <variant>

#include <boost/algorithm/string/predicate.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/map.hpp>

#include "get_me/formatting.hpp"
#include "get_me/graph.hpp"
#include "get_me/graph_types.hpp"
#include "get_me/tooling.hpp"
#include "get_me/utility.hpp"

// NOLINTBEGIN
using ranges::views::filter;

using namespace llvm::cl;

static OptionCategory ToolCategory("get_me");
static opt<std::string> TypeName("t", desc("Name of the type to get"), Required,
                                 ValueRequired, cat(ToolCategory));
// NOLINTEND

using TypeSetTransitionDataType =
    std::tuple<TypeSet, TransitionDataType, TypeSet>;

[[nodiscard]] static std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FunctionDecl *FDecl) {
  const auto Parameters = FDecl->parameters();
  auto ParameterTypeRange =
      Parameters |
      ranges::views::transform(
          [](const clang::ParmVarDecl *PVDecl) { return PVDecl->getType(); });
  return {{FDecl->getReturnType()},
          {ParameterTypeRange.begin(), ParameterTypeRange.end()}};
}

[[nodiscard]] static std::pair<TypeSet, TypeSet>
toTypeSet(const clang::FieldDecl *FDecl) {
  return {{TypeSetValueType{FDecl->getType()}},
          {TypeSetValueType{FDecl->getParent()}}};
}

[[nodiscard]] static std::vector<TypeSetTransitionDataType>
getTypeSetTransitionData(const TransitionCollector &Collector) {
  std::vector<TypeSetTransitionDataType> TypeSetTransitionData{};
  TypeSetTransitionData.reserve(Collector.Data.size());
  ranges::transform(
      Collector.Data, std::back_inserter(TypeSetTransitionData),
      [](const auto &Val) {
        return std::visit(
            Overloaded{[&Val](const auto *Decl) -> TypeSetTransitionDataType {
              auto [Acquired, Required] = toTypeSet(Decl);
              return {{std::move(Acquired)}, Val, std::move(Required)};
            }},
            Val);
      });
  return TypeSetTransitionData;
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

  GetMeGraphData GraphData{};

  TransitionCollector Collector{};
  GetMeActionFactory Factory{Collector};
  auto Action = std::make_unique<GetMeAction>(Collector);
  if (const auto ToolRunResult = Tool.run(&Factory); ToolRunResult != 0) {
    spdlog::error("error while running tool");
    return 0;
  }

  fmt::print("{}\n", Collector.Data);

  // build graph from gathered graph data
  const auto TypeSetTransitionData = getTypeSetTransitionData(Collector);
  fmt::print("TypeSetTransitionData size: {}\n", TypeSetTransitionData.size());

  for (const auto &[Acquired, Transition, Required] : TypeSetTransitionData) {
    if (const auto Iter = ranges::find_if(
            Acquired,
            [](const TypeSetValueType &Val) {
              auto Name = TypeName.getValue();
              auto NameWithoutNull = std::span{Name.begin(), Name.end() - 1};
              return std::visit(
                  Overloaded{
                      [NameWithoutNull](const clang::NamedDecl *NDecl) {
                        return NDecl->getName().contains(llvm::StringRef{
                            NameWithoutNull.data(), NameWithoutNull.size()});
                      },
                      [&Name](const clang::QualType *QType) {
                        return boost::contains(QType->getAsString(), Name);
                      },
                      [&Name](const clang::QualType &QType) {
                        return boost::contains(QType.getAsString(), Name);
                      }},
                  Val);
            });
        Iter != Acquired.end() &&
        !ranges::contains(GraphData.VertexData | ranges::views::values,
                          Acquired)) {
      GraphData.VertexData.try_emplace(GraphData.VertexData.size(), Acquired);
    }
  }

  fmt::print("GraphData.VertexData: {}\n", GraphData.VertexData);

  // for (size_t NumAddedTransitions = std::numeric_limits<size_t>::max();
  //      NumAddedTransitions != GraphData.Edges.size();) {
  //   NumAddedTransitions = 0U;
  //   fmt::print("Outer Iter {}\n", NumAddedTransitions);
  //   for (const auto &Transition : TypeSetTransitionData) {
  //     const auto &AcquiredTypeSet = std::get<0>(Transition);
  //     fmt::print("Transition: Acq: {}\n", AcquiredTypeSet);
  //     for (const auto &[VertexId, VertexTypeSet] :
  //          GraphData.VertexData | filter([&AcquiredTypeSet](
  //                                            const auto &KeyValue) {
  //            return ranges::includes(std::get<1>(KeyValue), AcquiredTypeSet);
  //          })) {
  //       fmt::print("found vertex to add to\n");
  //       ++NumAddedTransitions;
  //     }
  //   }
  // }

  GraphType Graph(GraphData.Edges.data(),
                  GraphData.Edges.data() + GraphData.Edges.size(),
                  GraphData.EdgeWeights.data(), GraphData.EdgeWeights.size());

  // const weight_map_type Weightmap = get(boost::edge_weight, Graph);

  // const auto SourceVertex =
  //     ranges::find_if(Collector.Data, [](const TransitionDataType &Val) {
  //       return std::visit(Overloaded{[](const clang::NamedDecl *NDecl) {
  //                           return NDecl->getNameAsString() ==
  //                                  TypeName.getValue();
  //                         }},
  //                         Val);
  //     });

  // const auto paths = pathTraversal(Graph, *SourceVertex);

  // for (const auto independent_paths_range = independentPaths(paths, Graph);
  //      const PathType<GraphType> &path : independent_paths_range) {
  //   fmt::print(
  //       "path: \n{}\n",
  //       fmt::join(
  //           path | ranges::views::transform([&Weightmap, &Graph,
  //                                            &GraphData](const auto &edge) {
  //             const edge_weight_type &edge_weight = get(Weightmap, edge);
  //             return fmt::format(
  //                 "{} {}({})",
  //                 GraphData.VertexData.find(source(edge, Graph))->second,
  //                 std::visit(Overloaded{[](const clang::NamedDecl *NDecl) {
  //                              return NDecl->getNameAsString();
  //                            }},
  //                            edge_weight),
  //                 GraphData.VertexData.find(target(edge, Graph))->second);
  //           }),
  //           "\n"));
  // }

  // std::ofstream DotFile("dijkstra-eg.dot");
  // fmt::print(DotFile, "digraph D {{\n");

  // for (const auto &Edge : toRange(edges(Graph))) {
  //   const auto SourceNode = source(Edge, Graph);
  //   const auto TargetNode = target(Edge, Graph);
  //   const auto TargetName = get_name(TargetNode);
  //   const auto SourceName = get_name(SourceNode);
  //   fmt::print(DotFile, "  \"{}\" -> \"{}\"[label=\"{}\"]\n", SourceName,
  //              TargetName, get(Weightmap, Edge));
  // }

  // fmt::print(DotFile, "}}");
}

#include <fstream>
#include <variant>

#include <clang/Tooling/CommonOptionsParser.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <llvm/Support/Signals.h>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

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

  const auto TypeSetTransitionData = getTypeSetTransitionData(Collector);

  const auto &QueriedType = TypeName.getValue();

  auto Data = generateVertexAndEdgeWeigths(TypeSetTransitionData, QueriedType);

  // FIXME: apply greedy transition traversal strategy

  spdlog::info("Data sizes:\n\tVertexData: {}\n\tEdges: {}\n\tEdgeWeights: "
               "{}\n\tEdgeWeightMap: {}",
               Data.VertexData.size(), Data.Edges.size(),
               Data.EdgeWeights.size(), Data.EdgeWeightMap.size());

  GraphType Graph(Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
                  Data.EdgeWeights.data(), Data.EdgeWeights.size());

  std::ofstream DotFile("graph.dot");
  std::string Res{};
  Res += fmt::format("digraph D {{\n");

  for (const auto &Edge : toRange(edges(Graph))) {
    const auto SourceNode = source(Edge, Graph);
    const auto TargetNode = target(Edge, Graph);

    const auto EdgeWeight = Data.EdgeWeightMap.at({SourceNode, TargetNode});
    const auto TargetVertex = Data.VertexData[TargetNode];
    const auto SourceVertex = Data.VertexData[SourceNode];

    auto FormattedEdge = fmt::format(
        R"(  "{}" -> "{}"[label="{}"]
)",
        SourceVertex, TargetVertex, EdgeWeight);
    Res += FormattedEdge;
  }

  Res += fmt::format("}}");
  DotFile << Res;

  const auto SourceVertexDesc =
      getSourceVertexMatchingQueriedType(Data, QueriedType);
  const auto Paths = pathTraversal(Graph, SourceVertexDesc);

  for (const auto [Path, Number] : views::zip(Paths, views::iota(0U))) {
    spdlog::info(
        "path #{}: {}", Number,
        fmt::join(
            Path | views::transform([&Graph, &Data](const auto &Transition) {
              const auto Edge = std::pair{source(Transition, Graph),
                                          target(Transition, Graph)};
              const auto Weight = Data.EdgeWeightMap.at(Edge);
              const auto TargetName = getTransitionAcquiredTypeNames(Weight);
              const auto SourceName = getTransitionRequiredTypeNames(Weight);
              return Weight;
            }),
            ", "));
  }
}

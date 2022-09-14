#include <fstream>
#include <variant>

#include <boost/algorithm/string/replace.hpp>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <llvm/Support/Signals.h>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/cfg/env.h>
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

  spdlog::cfg::load_env_levels();

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

  TransitionCollector TypeSetTransitionData{};
  std::vector<std::unique_ptr<clang::ASTUnit>> ASTs{};
  if (const auto BuildASTsResult = Tool.buildASTs(ASTs); BuildASTsResult != 0) {
    spdlog::error("error building ASTs");
    return 1;
  }
  auto Conf = getDefaultConfig();
  Conf.EnableArithmeticTruncation = true;
  auto Consumer = GetMe{Conf, TypeSetTransitionData};
  for (const auto &AST : ASTs) {
    Consumer.HandleTranslationUnit(AST->getASTContext());
  }

  const auto &QueriedType = TypeName.getValue();

  const auto [Graph, Data] = createGraph(TypeSetTransitionData, QueriedType);
  const auto IndexMap = boost::get(boost::edge_index, Graph);

  // FIXME: apply greedy transition traversal strategy

  spdlog::info("Graph size: |V| = {}, |E| = {}", Data.VertexData.size(),
               Data.Edges.size());

  std::ofstream DotFile("graph.dot");
  std::string Res{};
  Res += fmt::format("digraph D {{\n  layout = \"sfdp\";\n");

  for (const auto &Edge : toRange(edges(Graph))) {
    const auto SourceNode = source(Edge, Graph);
    const auto TargetNode = target(Edge, Graph);

    const auto EdgeWeight = Data.EdgeWeights[boost::get(IndexMap, Edge)];
    const auto TargetVertex = Data.VertexData[TargetNode];
    const auto SourceVertex = Data.VertexData[SourceNode];

    auto EdgeWeightAsString = toString(EdgeWeight);
    boost::replace_all(EdgeWeightAsString, "\"", "\\\"");
    auto FormattedEdge = fmt::format(
        R"(  "{}" -> "{}"[label="{}"]
)",
        SourceVertex, TargetVertex, EdgeWeightAsString);
    Res += FormattedEdge;
  }

  Res += fmt::format("}}");
  DotFile << Res;

  const auto SourceVertexDesc =
      getSourceVertexMatchingQueriedType(Data, QueriedType);
  if (!SourceVertexDesc) {
    return 1;
  }
  auto Paths = pathTraversal(Graph, *SourceVertexDesc);
  ranges::sort(Paths, [](const auto &Lhs, const auto &Rhs) {
    return Lhs.size() < Rhs.size();
  });

  for (const auto [Path, Number] :
       views::zip(Paths | ranges::views::take(25), views::iota(0U))) {
    spdlog::info(
        "path #{}: {}", Number,
        fmt::join(Path | views::transform([&Data, &IndexMap](
                                              const EdgeDescriptor &Edge) {
                    return toString(
                        Data.EdgeWeights[boost::get(IndexMap, Edge)]);
                  }),
                  ", "));
  }
}

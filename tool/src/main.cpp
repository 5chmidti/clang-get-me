#include <compare>
#include <fstream>
#include <variant>

#include <boost/algorithm/string/replace.hpp>
#include <clang/AST/DeclCXX.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <llvm/Support/Signals.h>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/partial_sort.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
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
  const auto Conf = Config{
      .EnableFilterOverloads = true,
      .EnablePropagateInheritance = true,
      .EnablePropagateTypeAlias = true,
      .EnableTruncateArithmetic = true,
      .EnableFilterStd = true,
      .MaxGraphDepth = 6,
      .MaxPathCount = 10000,
  };
  for (const auto &AST : ASTs) {
    GetMe{Conf, TypeSetTransitionData, AST->getSema()}.HandleTranslationUnit(
        AST->getASTContext());
  }

  const auto &QueriedType = TypeName.getValue();

  // workaround for lmdba captures in clang
  const auto GraphAndData =
      createGraph(TypeSetTransitionData, QueriedType, Conf);
  const auto &Graph = GraphAndData.first;
  const auto &Data = GraphAndData.second;
  const auto IndexMap = boost::get(boost::edge_index, Graph);

  spdlog::info("Graph size: |V| = {}, |E| = {}", Data.VertexData.size(),
               Data.Edges.size());

  std::ofstream DotFile("graph.dot");
  std::string Res{};
  Res += fmt::format("digraph D {{\n  layout = \"sfdp\";\n");

  for (const auto &Edge : toRange(boost::edges(Graph))) {
    const auto SourceNode = boost::source(Edge, Graph);
    const auto TargetNode = boost::target(Edge, Graph);

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
  auto Paths = pathTraversal(Graph, Data, Conf, *SourceVertexDesc);
  const auto OutputPathCount = 50U;
  ranges::partial_sort(
      Paths, Paths.begin() + OutputPathCount,
      [&Data, &Graph](const PathType &Lhs, const PathType &Rhs) {
        if (const auto Comp = Lhs.size() <=> Rhs.size(); std::is_neq(Comp)) {
          return std::is_lt(Comp);
        }
        if (Lhs.empty()) {
          return true;
        }
        return Data.VertexData[target(Lhs.back(), Graph)].size() <
               Data.VertexData[target(Rhs.back(), Graph)].size();
      });

  for (const auto [Path, Number] : views::zip(
           Paths | ranges::views::take(OutputPathCount), views::iota(0U))) {
    spdlog::info(
        "path #{}: {} -> remaining: {}", Number,
        fmt::join(Path | views::transform([&Data, &IndexMap](
                                              const EdgeDescriptor &Edge) {
                    return toString(
                        Data.EdgeWeights[boost::get(IndexMap, Edge)]);
                  }),
                  ", "),
        Data.VertexData[boost::target(Path.back(), Graph)]);
  }
}

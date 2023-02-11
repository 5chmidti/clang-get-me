#include <algorithm>
#include <compare>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string/replace.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/property_map/property_map.hpp>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/os.h>
#include <fmt/ranges.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/raw_ostream.h>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/access.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/single.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/view.hpp>
#include <spdlog/cfg/env.h>
#include <spdlog/common.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "get_me/graph.hpp"
#include "get_me/path_traversal.hpp"
#include "get_me/query.hpp"
#include "get_me/tooling.hpp"
#include "get_me/transitions.hpp"
#include "query_all/query_all.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/ranges.hpp"
#include "tui/tui.hpp"

// NOLINTBEGIN
using namespace llvm::cl;

static OptionCategory ToolCategory("get_me");
static opt<std::string> TypeName("t", desc("Name of the type to get"), Required,
                                 ValueRequired, cat(ToolCategory));
static opt<std::string> ConfigPath("config", desc("Config file path"), Optional,
                                   ValueRequired, cat(ToolCategory));
static opt<bool> Verbose("v", desc("Verbose output"), Optional,
                         cat(ToolCategory));
static opt<bool> Interactive("i", desc("Run with interactive gui"), Optional,
                             cat(ToolCategory));
// NOLINTEND

namespace {
void dumpToDotFile(const GraphType &Graph, const GraphData &Data) {
  const auto IndexMap = boost::get(boost::edge_index, Graph);

  auto DotFile = fmt::output_file("graph.dot");
  DotFile.print("digraph D {{\n  layout = \"sfdp\";\n");

  for (const auto &Edge : toRange(boost::edges(Graph))) {
    const auto SourceNode = Source(Edge);
    const auto TargetNode = Target(Edge);

    const auto Transition =
        ToTransition(Data.EdgeWeights[boost::get(IndexMap, Edge)]);
    const auto TargetVertex = Data.VertexData[TargetNode];
    const auto SourceVertex = Data.VertexData[SourceNode];

    auto EdgeWeightAsString = fmt::format("{}", Transition);
    boost::replace_all(EdgeWeightAsString, "\"", "\\\"");
    DotFile.print(
        R"(  "{}" -> "{}"[label="{}"]
)",
        SourceVertex, TargetVertex, EdgeWeightAsString);
  }
  DotFile.print("}}\n");
}
} // namespace

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  auto FileSink =
      std::make_shared<spdlog::sinks::basic_file_sink_mt>("get_me_log.txt");
  FileSink->set_level(spdlog::level::trace);
  spdlog::default_logger()->sinks().push_back(FileSink);

  spdlog::cfg::load_env_levels();

  auto OptionsParser =
      clang::tooling::CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!OptionsParser) {
    llvm::errs() << OptionsParser.takeError();
    return 1;
  }
  const auto &Sources = [&OptionsParser] {
    const auto &Result = OptionsParser->getSourcePathList();
    return Result.empty() ? OptionsParser->getCompilations().getAllFiles()
                          : Result;
  }();

  clang::tooling::ClangTool Tool(OptionsParser->getCompilations(), Sources);

  if (Verbose) {
    Tool.appendArgumentsAdjuster(
        [](const clang::tooling::CommandLineArguments &Args,
           const llvm::StringRef /*Filename*/) {
          auto Res = Args;
          Res.emplace_back("-v");
          return Res;
        });
  }

  const auto ConfigFilePath = std::filesystem::path{ConfigPath.getValue()};
  auto Conf = ConfigFilePath.empty() ? Config{} : Config::parse(ConfigFilePath);

  if (Interactive) {
    runTui(Conf, Tool);
    return 0;
  }

  auto TypeSetTransitionData = std::make_shared<TransitionCollector>();
  std::vector<std::unique_ptr<clang::ASTUnit>> ASTs{};
  const auto BuildASTsResult = Tool.buildASTs(ASTs);
  GetMeException::verify(BuildASTsResult == 0, "Error building ASTs");

  for (const auto &AST : ASTs) {
    collectTransitions(TypeSetTransitionData, *AST, Conf);
  }

  const auto &QueriedType = TypeName.getValue();
  const auto Query =
      QueryType{std::move(TypeSetTransitionData), QueriedType, Conf};

  // workaround for lmdba captures in clang
  const auto GraphAndData = createGraph(Query);
  const auto &Graph = GraphAndData.first;
  const auto &Data = GraphAndData.second;
  const auto IndexMap = boost::get(boost::edge_index, Graph);

  spdlog::info("Graph size: |V| = {}, |E| = {}", Data.VertexData.size(),
               Data.Edges.size());

  dumpToDotFile(Graph, Data);

  const auto SourceVertexDesc =
      getSourceVertexMatchingQueriedType(Data, Query.getQueriedType());
  auto Paths = pathTraversal(Graph, Data, Conf, SourceVertexDesc);
  spdlog::info(
      "path length distribution: {}",
      Paths |
          ranges::views::chunk_by([](const PathType &Lhs, const PathType &Rhs) {
            return Lhs.size() == Rhs.size();
          }) |
          ranges::views::transform([](const auto Range) {
            return std::pair{ranges::size(Range), ranges::begin(Range)->size()};
          }));
  auto PreIndepPathsSize = Paths.size();
  spdlog::info("generated {} paths", PreIndepPathsSize);

  const auto OutputPathCount = std::min<size_t>(Paths.size(), 10U);
  ranges::partial_sort(
      Paths,
      Paths.begin() +
          static_cast<std::vector<PathType>::difference_type>(OutputPathCount),
      [&Data](const PathType &Lhs, const PathType &Rhs) {
        if (const auto Comp = Lhs.size() <=> Rhs.size(); std::is_neq(Comp)) {
          return std::is_lt(Comp);
        }
        if (Lhs.empty()) {
          return true;
        }
        return Data.VertexData[Target(Lhs.back())].size() <
               Data.VertexData[Target(Rhs.back())].size();
      });

  ranges::for_each(
      Paths | ranges::views::take(OutputPathCount) | ranges::views::enumerate,
      [&Data, &IndexMap](const auto IndexedPath) {
        const auto &[Number, Path] = IndexedPath;
        spdlog::info(
            "path #{}: {} -> remaining: {}", Number,
            fmt::join(
                Path |
                    ranges::views::transform([&Data, &IndexMap](
                                                 const EdgeDescriptor &Edge) {
                      return fmt::format(
                          "{}",
                          ToTransition(
                              Data.EdgeWeights[boost::get(IndexMap, Edge)]));
                    }),
                ", "),
            Data.VertexData[Target(Path.back())]);
      });
}

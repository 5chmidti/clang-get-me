#include <clang/Tooling/CommonOptionsParser.h>
#include <fmt/format.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>

#include "get_me/formatting.hpp"
#include "get_me/graph.hpp"
#include "get_me/graph_types.hpp"
#include "get_me/tooling.hpp"

// NOLINTBEGIN
using namespace llvm::cl;

static OptionCategory ToolCategory("get_me");
static opt<std::string> TypeName("t", desc("Name of the type to get"), Required,
                                 ValueRequired, cat(ToolCategory));
// NOLINTEND

using edge_weight_type = LinkType;
using GraphType = boost::adjacency_list<
    boost::listS, boost::vecS, boost::directedS, boost::no_property,
    boost::property<boost::edge_weight_t, edge_weight_type>>;

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

  GraphData<GraphType, TypeSet, TransitionDataType> GraphData{};

  TransitionCollector Collector{};
  GetMeActionFactory Factory{Collector};
  auto Action = std::make_unique<GetMeAction>(Collector);
  if (const auto ToolRunResult = Tool.run(&Factory); ToolRunResult != 0) {
    spdlog::error("error while running tool");
    return 0;
  }

  fmt::print("{}\n", Collector.Data);

  // build graph from gathered graph data
}

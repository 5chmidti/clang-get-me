#include <algorithm>
#include <compare>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/os.h>
#include <fmt/ranges.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/raw_ostream.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/partial_sort.hpp>
#include <range/v3/functional/bind_back.hpp>
#include <range/v3/range/access.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/cfg/env.h>
#include <spdlog/common.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "get_me/backwards_path_finding.hpp"
#include "get_me/config.hpp"
#include "get_me/graph.hpp"
#include "get_me/query.hpp"
#include "get_me/query_all.hpp"
#include "get_me/tooling.hpp"
#include "get_me/transitions.hpp"
#include "support/get_me_exception.hpp"
#include "tui/tui.hpp"

// NOLINTBEGIN
using namespace llvm::cl;

static OptionCategory ToolCategory("get_me");
const static opt<std::string> TypeName("t", desc("Name of the type to get"),
                                       ValueRequired, cat(ToolCategory));
const static opt<std::string> ConfigPath("config", desc("Config file path"),
                                         ValueRequired, cat(ToolCategory));
const static opt<bool> Verbose("v", desc("Verbose output"), cat(ToolCategory));
const static opt<bool> Interactive("i", desc("Run with interactive gui"),
                                   cat(ToolCategory));
const static opt<bool>
    QueryAll("query-all",
             desc("Query every type available (that has a transition)"),
             cat(ToolCategory));
// NOLINTEND

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
  const auto &Sources = OptionsParser->getSourcePathList();

  GetMeException::verify(ranges::size(Sources) == 1,
                         "Built {} ASTs, expected 1", ranges::size(Sources));

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
  auto Conf = std::make_shared<Config>(
      ConfigFilePath.empty() ? Config{} : Config::parse(ConfigFilePath));

  if (Interactive) {
    runTui(Conf, Tool);
    return 0;
  }

  std::vector<std::unique_ptr<clang::ASTUnit>> ASTs{};
  const auto BuildASTsResult = Tool.buildASTs(ASTs);
  GetMeException::verify(BuildASTsResult == 0, "Error building ASTs");

  auto Transitions = collectTransitions(*ASTs.front(), Conf);

  if (QueryAll) {
    queryAll(Transitions, Conf);
    return 0;
  }

  const auto &QueriedType = TypeName.getValue();
  const auto Query = getQueriedTypesForInput(*Transitions, QueriedType);

  auto Data = runGraphBuilding(Transitions, Query, Conf);
  {
    auto DotFile = fmt::output_file("graph.dot");
    DotFile.print("{:d}", Data);
  }
  auto Paths = runPathFinding(Data) | ranges::to_vector |
               ranges::actions::sort(ranges::less{}, ranges::size);

  spdlog::info("|Transitions|: {}", ranges::size(Data.Transitions->FlatData));
  spdlog::info("Graph size: |V| = {}, |E| = {}", Data.VertexData.size(),
               Data.Edges.size());

  spdlog::info(
      "path length distribution: {}",
      Paths |
          ranges::views::chunk_by([](const PathType &Lhs, const PathType &Rhs) {
            return Lhs.size() == Rhs.size();
          }) |
          ranges::views::transform([](const auto Range) {
            return std::pair{ranges::size(Range), ranges::begin(Range)->size()};
          }));
  spdlog::info("generated {} paths", Paths.size());

  const auto OutputPathCount =
      std::min<size_t>(Paths.size(), Conf->MaxPathOutputCount);
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
      Paths |
          ranges::views::for_each(
              ranges::bind_back(expandAndFlattenPath, Data)) |
          ranges::views::enumerate | ranges::views::take(OutputPathCount),
      [](const auto IndexedPath) {
        const auto &[Number, Path] = IndexedPath;
        spdlog::info(
            "path #{}: {} -> remaining: {}", Number,
            fmt::join(Path | ranges::views::transform(ToTransition), ", "),
            ToRequired(Path.back()));
      });
}

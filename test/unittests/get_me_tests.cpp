#include "get_me_tests.hpp"

#include <source_location>
#include <string_view>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <get_me/formatting.hpp>
#include <get_me/tooling.hpp>
#include <gtest/gtest.h>
#include <range/v3/algorithm/permutation.hpp>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

std::tuple<GraphData, GraphType, std::vector<PathType>>
prepare(std::string_view Code, std::string_view QueriedType) {
  const auto AST =
      clang::tooling::buildASTFromCodeWithArgs(Code, {"-std=c++20"});

  TransitionCollector TypeSetTransitionData{};
  auto Consumer = GetMe{TypeSetTransitionData};
  Consumer.HandleTranslationUnit(AST->getASTContext());
  const auto QueriedTypeAsString = std::string{QueriedType};
  const auto [Graph, Data] =
      createGraph(TypeSetTransitionData, QueriedTypeAsString);
  const auto SourceVertex =
      getSourceVertexMatchingQueriedType(Data, QueriedTypeAsString);
  const auto FoundPaths = pathTraversal(Graph, SourceVertex);
  return {Data, Graph, FoundPaths};
}

void test(std::string_view Code, std::string_view QueriedType,
          const std::vector<std::string_view> &ExpectedPaths,
          std::source_location Loc) {
  const auto [Data, Graph, FoundPaths] = prepare(Code, QueriedType);

  const auto FoundPathsAsString = toString(FoundPaths, Graph, Data);

  testing::ScopedTrace trace(Loc.file_name(), static_cast<int>(Loc.line()),
                             "Test source");
  EXPECT_TRUE(ranges::is_permutation(FoundPathsAsString, ExpectedPaths))
      << fmt::format("Expected: {}\nFound: {}", ExpectedPaths,
                     FoundPathsAsString);
}

GetMeTest::GetMeTest() : testing::Test{} {
  spdlog::set_level(spdlog::level::debug);
  static constexpr auto BacktraceCount = 1024U;
  spdlog::enable_backtrace(BacktraceCount);
}

void GetMeTest::TearDown() {
  if (HasFailure()) {
    spdlog::dump_backtrace();
  }
}

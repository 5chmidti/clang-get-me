#include "get_me_tests.hpp"

#include <functional>
#include <iterator>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/format.h>
#include <get_me/formatting.hpp>
#include <get_me/graph.hpp>
#include <get_me/tooling.hpp>
#include <gtest/gtest.h>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

void test(std::string_view Code, std::string_view QueriedType,
          std::vector<std::string_view> ExpectedPaths,
          std::source_location Loc) {
  testing::ScopedTrace trace(Loc.file_name(), static_cast<int>(Loc.line()),
                             "Test source");

  ranges::sort(ExpectedPaths);

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
  const auto VertexDataSize = Data.VertexData.size();
  // adjusted for empty set
  ASSERT_LT(SourceVertex, VertexDataSize - 1);
  const auto FoundPaths = pathTraversal(Graph, SourceVertex);

  auto FoundPathsAsString = toString(FoundPaths, Graph, Data);
  ranges::sort(FoundPathsAsString);
  const auto ToString = [](const auto &Val) { return std::string{Val}; };
  const auto ToSetDifference = [&ToString](const auto &Lhs, const auto &Rhs) {
    std::vector<std::string> Res{};
    ranges::set_difference(Lhs, Rhs, std::back_inserter(Res), std::less{},
                           ToString, ToString);
    return Res;
  };

  EXPECT_TRUE(ranges::is_permutation(FoundPathsAsString, ExpectedPaths))
      << fmt::format(
             "Expected: {}\nFound: {}\nNot found: {}\nNot expected: {}",
             ExpectedPaths, FoundPathsAsString,
             ToSetDifference(ExpectedPaths | ranges::views::transform(ToString),
                             FoundPathsAsString),
             ToSetDifference(FoundPathsAsString,
                             ExpectedPaths |
                                 ranges::views::transform(ToString)));
}

GetMeTest::GetMeTest() {
  spdlog::set_level(spdlog::level::debug);
  static constexpr auto BacktraceCount = 1024U;
  spdlog::enable_backtrace(BacktraceCount);
}

void GetMeTest::TearDown() {
  if (HasFailure()) {
    spdlog::dump_backtrace();
  }
}

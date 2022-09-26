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
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

void test(std::string_view Code, std::string_view QueriedType,
          const std::set<std::string_view> &ExpectedPaths,
          const Config &CurrentConfig, std::source_location Loc) {
  const testing::ScopedTrace Trace(Loc.file_name(),
                                   static_cast<int>(Loc.line()), "Test source");
  spdlog::trace("{:*^100}", fmt::format("Test start ({}:{})",
                                        Loc.function_name(), Loc.line()));

  const auto AST =
      clang::tooling::buildASTFromCodeWithArgs(Code, {"-std=c++20"});

  TransitionCollector TypeSetTransitionData{};
  auto Consumer = GetMe{CurrentConfig, TypeSetTransitionData, AST->getSema()};
  Consumer.HandleTranslationUnit(AST->getASTContext());
  const auto QueriedTypeAsString = std::string{QueriedType};
  const auto [Graph, Data] =
      createGraph(TypeSetTransitionData, QueriedTypeAsString, CurrentConfig);
  const auto SourceVertex =
      getSourceVertexMatchingQueriedType(Data, QueriedTypeAsString);
  const auto VertexDataSize = Data.VertexData.size();
  ASSERT_TRUE(SourceVertex.has_value());
  if (!SourceVertex.has_value()) {
    return;
  }
  // adjusted for empty set
  ASSERT_LT(SourceVertex, VertexDataSize - 1);
  const auto FoundPaths =
      pathTraversal(Graph, Data, CurrentConfig, *SourceVertex);

  const auto FoundPathsAsString =
      ranges::to<std::set>(toString(FoundPaths, Graph, Data));
  const auto ToString = []<typename T>(const T &Val) -> std::string {
    if constexpr (std::is_same_v<T, std::string>) {
      return Val;
    }
    return std::string{Val};
  };
  const auto ToSetDifference = [&ToString](const auto &Lhs, const auto &Rhs) {
    std::set<std::string> Res{};
    ranges::set_difference(Lhs, Rhs, std::inserter(Res, Res.end()), std::less{},
                           ToString, ToString);
    return Res;
  };

  EXPECT_TRUE(ranges::equal(FoundPathsAsString, ExpectedPaths)) << fmt::format(
      "Expected: {}\nFound: {}\nNot found: {}\nNot expected: {}", ExpectedPaths,
      FoundPathsAsString,
      ToSetDifference(ExpectedPaths | ranges::views::transform(ToString),
                      FoundPathsAsString),
      ToSetDifference(FoundPathsAsString,
                      ExpectedPaths | ranges::views::transform(ToString)));
}

GetMeTest::GetMeTest() {
  spdlog::set_level(spdlog::level::trace);
  static constexpr auto BacktraceCount = 1024U;
  spdlog::enable_backtrace(BacktraceCount);
}

void GetMeTest::TearDown() {
  if (HasFailure()) {
    spdlog::dump_backtrace();
  }
}

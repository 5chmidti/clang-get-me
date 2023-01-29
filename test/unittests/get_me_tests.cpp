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
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define VERIFY(EXPECT_OUTCOME, FoundPathsAsString, ExpectedPaths)              \
  {                                                                            \
    const auto ToString = []<typename T>(const T &Val) -> std::string {        \
      if constexpr (std::is_same_v<T, std::string>) {                          \
        return Val;                                                            \
      }                                                                        \
      return std::string{Val};                                                 \
    };                                                                         \
                                                                               \
    const auto ToSetDifference = [&ToString](const auto &Lhs,                  \
                                             const auto &Rhs) {                \
      return ranges::views::set_difference(Lhs, Rhs, std::less{}, ToString,    \
                                           ToString) |                         \
             ranges::to<std::set>;                                             \
    };                                                                         \
                                                                               \
    EXPECT_OUTCOME(ranges::equal(FoundPathsAsString, ExpectedPaths))           \
        << fmt::format("Expected:\t{}\nFound:\t\t{}\nNot found:\t{}\nNot "     \
                       "expected:\t{}",                                        \
                       ExpectedPaths, FoundPathsAsString,                      \
                       ToSetDifference(ExpectedPaths, FoundPathsAsString),     \
                       ToSetDifference(FoundPathsAsString, ExpectedPaths));    \
  }

GetMeTest::GetMeTest() {
  spdlog::set_level(spdlog::level::trace);
  static constexpr auto BacktraceCount = 1024U;
  spdlog::enable_backtrace(BacktraceCount);
}

void GetMeTest::SetUp() { Config_ = CurrentConfig_; }

void GetMeTest::TearDown() {
  if (HasFailure()) {
    spdlog::dump_backtrace();
  }
  CurrentConfig_ = Config_;
}

void GetMeTest::test(const std::string_view Code,
                     const std::string_view QueriedType,
                     const std::set<std::string, std::less<>> &ExpectedPaths,
                     const std::source_location Loc) const {
  const testing::ScopedTrace Trace(Loc.file_name(),
                                   static_cast<int>(Loc.line()), "Test source");
  spdlog::trace("{:*^100}", fmt::format("Test start ({}:{})",
                                        Loc.function_name(), Loc.line()));

  const auto AST =
      clang::tooling::buildASTFromCodeWithArgs(Code, {"-std=c++20"});

  auto TypeSetTransitionData = std::make_shared<TransitionCollector>();
  auto Consumer = GetMe{CurrentConfig_, *TypeSetTransitionData, AST->getSema()};
  Consumer.HandleTranslationUnit(AST->getASTContext());
  const auto Query = QueryType{std::move(TypeSetTransitionData),
                               std::string{QueriedType}, CurrentConfig_};
  const auto [Graph, Data] = createGraph(Query);
  const auto SourceVertex =
      getSourceVertexMatchingQueriedType(Data, Query.getQueriedType());
  const auto VertexDataSize = Data.VertexData.size();
  // adjusted for empty set
  ASSERT_LT(SourceVertex, VertexDataSize - 1);
  const auto FoundPaths =
      pathTraversal(Graph, Data, CurrentConfig_, SourceVertex);

  const auto FoundPathsAsString =
      toString(FoundPaths, Graph, Data) | ranges::to<std::set>;

  VERIFY(EXPECT_TRUE, FoundPathsAsString, ExpectedPaths)
}

void GetMeTest::testFailure(
    const std::string_view Code, const std::string_view QueriedType,
    const std::set<std::string, std::less<>> &ExpectedPaths,
    const std::source_location Loc) const {
  const testing::ScopedTrace Trace(Loc.file_name(),
                                   static_cast<int>(Loc.line()), "Test source");
  spdlog::trace("{:*^100}", fmt::format("Test start ({}:{})",
                                        Loc.function_name(), Loc.line()));

  const auto AST =
      clang::tooling::buildASTFromCodeWithArgs(Code, {"-std=c++20"});

  auto TypeSetTransitionData = std::make_shared<TransitionCollector>();
  auto Consumer = GetMe{CurrentConfig_, *TypeSetTransitionData, AST->getSema()};
  Consumer.HandleTranslationUnit(AST->getASTContext());
  const auto Query = QueryType{std::move(TypeSetTransitionData),
                               std::string{QueriedType}, CurrentConfig_};
  const auto [Graph, Data] = createGraph(Query);
  const auto SourceVertex =
      getSourceVertexMatchingQueriedType(Data, Query.getQueriedType());
  const auto VertexDataSize = Data.VertexData.size();
  // adjusted for empty set
  ASSERT_LT(SourceVertex, VertexDataSize - 1);
  const auto FoundPaths =
      pathTraversal(Graph, Data, CurrentConfig_, SourceVertex);

  const auto FoundPathsAsString =
      toString(FoundPaths, Graph, Data) | ranges::to<std::set>;

  VERIFY(EXPECT_FALSE, FoundPathsAsString, ExpectedPaths)
}

void GetMeTest::testNoThrow(const std::string_view Code,
                            const std::string_view QueriedType,
                            const std::source_location Loc) const {
  const auto Test = [this, &Loc, &Code, &QueriedType]() {
    const testing::ScopedTrace Trace(
        Loc.file_name(), static_cast<int>(Loc.line()), "Test source");
    spdlog::trace("{:*^100}", fmt::format("Test start ({}:{})",
                                          Loc.function_name(), Loc.line()));

    const auto AST =
        clang::tooling::buildASTFromCodeWithArgs(Code, {"-std=c++20"});

    auto TypeSetTransitionData = std::make_shared<TransitionCollector>();
    auto Consumer =
        GetMe{CurrentConfig_, *TypeSetTransitionData, AST->getSema()};
    Consumer.HandleTranslationUnit(AST->getASTContext());
    const auto Query = QueryType{std::move(TypeSetTransitionData),
                                 std::string{QueriedType}, CurrentConfig_};
    const auto [Graph, Data] = createGraph(Query);
    const auto SourceVertex =
        getSourceVertexMatchingQueriedType(Data, Query.getQueriedType());
    const auto VertexDataSize = Data.VertexData.size();
    // adjusted for empty set
    ASSERT_LT(SourceVertex, VertexDataSize - 1);
    const auto FoundPaths =
        pathTraversal(Graph, Data, CurrentConfig_, SourceVertex);

    const auto FoundPathsAsString =
        toString(FoundPaths, Graph, Data) | ranges::to<std::set>;
  };

  ASSERT_NO_THROW(Test());
}

#include "get_me_tests.hpp"

#include <functional>
#include <memory>
#include <set>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <get_me/graph.hpp>
#include <get_me/tooling.hpp>
#include <oneapi/tbb/parallel_for_each.h>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/query.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"

namespace {
constexpr auto BacktraceSize = 1024U;

[[nodiscard]] std::string toString(const std::source_location &Loc) {
  return fmt::format("Source: {}:{}:{}", Loc.file_name(), Loc.line(),
                     Loc.column());
}

void verify(const bool ExpectedEqualityResult, const auto &FoundPathsAsString,
            const auto &ExpectedPaths) {
  const auto ToString = []<typename T>(const T &Val) -> std::string {
    if constexpr (std::is_same_v<T, std::string>) {
      return Val;
    }
    return std::string{Val};
  };

  const auto ToSetDifference = [&ToString](const auto &Lhs, const auto &Rhs) {
    return ranges::views::set_difference(Lhs, Rhs, std::less{}, ToString,
                                         ToString) |
           ranges::to<std::set>;
  };
  static constexpr auto Indentation = 14;
  static const auto Seperator = fmt::format("\n{0: <{1}}  ", "", Indentation);
  INFO(fmt::format(
      "{1: <{0}}: {2}\n"
      "{3: <{0}}: {4}\n"
      "{5: <{0}}: {6}\n"
      "{7: <{0}}: {8}\n",
      Indentation, "Expected", fmt::join(ExpectedPaths, Seperator), "Found",
      fmt::join(FoundPathsAsString, Seperator), "Not found",
      fmt::join(ToSetDifference(ExpectedPaths, FoundPathsAsString), Seperator),
      "Not expected",
      fmt::join(ToSetDifference(FoundPathsAsString, ExpectedPaths),
                Seperator)));
  REQUIRE(ExpectedEqualityResult ==
          ranges::equal(FoundPathsAsString, ExpectedPaths));
}
} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_LOC(Loc)                                                           \
  spdlog::trace(toString(Loc));                                                \
  INFO(toString(Loc))

void test(const std::string_view Code, const std::string_view QueriedType,
          const std::set<std::string, std::less<>> &ExpectedPaths,
          const Config &CurrentConfig, const std::source_location Loc) {
  testSuccess(Code, QueriedType, ExpectedPaths, CurrentConfig, Loc);
  testQueryAll(Code, CurrentConfig, Loc);
}

void testSuccess(const std::string_view Code,
                 const std::string_view QueriedType,
                 const std::set<std::string, std::less<>> &ExpectedPaths,
                 const Config &CurrentConfig, const std::source_location Loc) {
  spdlog::enable_backtrace(BacktraceSize);
  LOG_LOC(Loc);
  const auto [AST, Transitions] = collectTransitions(Code, CurrentConfig);
  const auto FoundPathsAsString =
      buildGraphAndFindPaths(Transitions, QueriedType, CurrentConfig);
  verify(true, FoundPathsAsString, ExpectedPaths);
  spdlog::disable_backtrace();
}

void testFailure(const std::string_view Code,
                 const std::string_view QueriedType,
                 const std::set<std::string, std::less<>> &ExpectedPaths,
                 const Config &CurrentConfig, const std::source_location Loc) {
  LOG_LOC(Loc);
  spdlog::enable_backtrace(BacktraceSize);
  const auto [AST, Transitions] = collectTransitions(Code, CurrentConfig);
  const auto FoundPathsAsString =
      buildGraphAndFindPaths(Transitions, QueriedType, CurrentConfig);
  verify(false, FoundPathsAsString, ExpectedPaths);
  spdlog::disable_backtrace();
}

void testNoThrow(const std::string_view Code,
                 const std::string_view QueriedType,
                 const Config &CurrentConfig, const std::source_location Loc) {
  spdlog::enable_backtrace(BacktraceSize);
  const auto [AST, Transitions] = collectTransitions(Code, CurrentConfig);
  const auto Test = [&Loc, &Transitions, &QueriedType, &CurrentConfig]() {
    LOG_LOC(Loc);
    std::ignore =
        buildGraphAndFindPaths(Transitions, QueriedType, CurrentConfig);
    spdlog::disable_backtrace();
  };

  REQUIRE_NOTHROW(Test());
}

void testQueryAll(const std::string_view Code, const Config &CurrentConfig,
                  const std::source_location Loc) {
  spdlog::enable_backtrace(BacktraceSize);
  LOG_LOC(Loc);

  const auto [AST, Transitions] = collectTransitions(Code, CurrentConfig);

  const auto Run = [&Transitions, &CurrentConfig](const auto &QueriedType) {
    REQUIRE_NOTHROW(
        std::ignore = buildGraphAndFindPaths(
            Transitions, fmt::format("{}", QueriedType), CurrentConfig));
  };

  tbb::parallel_for_each(
      Transitions->Data | ranges::views::transform(ToAcquired), Run);
  spdlog::disable_backtrace();
}

std::pair<std::unique_ptr<clang::ASTUnit>, std::shared_ptr<TransitionCollector>>
collectTransitions(const std::string_view Code, const Config &CurrentConfig) {
  auto AST = clang::tooling::buildASTFromCodeWithArgs(Code, {"-std=c++20"});
  auto Transitions = std::make_shared<TransitionCollector>();
  collectTransitions(Transitions, *AST, CurrentConfig);
  return {std::move(AST), std::move(Transitions)};
}

std::set<std::string>
buildGraphAndFindPaths(const std::shared_ptr<TransitionCollector> &Transitions,
                       const std::string_view QueriedType,
                       const Config &CurrentConfig) {
  const auto Query = getQueriedTypeForInput(Transitions->Data, QueriedType);
  auto Data = runGraphBuildingAndPathFinding(Transitions, Query, CurrentConfig);
  const auto SourceVertex = getSourceVertexMatchingQueriedType(Data, Query);
  const auto VertexDataSize = Data.VertexData.size();
  REQUIRE(VertexDataSize != 0);
  REQUIRE(SourceVertex < VertexDataSize);

  // return instead of requires because querying all might query a type with no
  // edges/transitions that acquire it
  if (Data.Edges.empty()) {
    return {};
  }

  return toString(Data.Paths, Data) | ranges::to<std::set>;
}

#include "get_me_tests.hpp"

#include <functional>
#include <iterator>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/format.h>
#include <get_me/formatting.hpp>
#include <get_me/graph.hpp>
#include <get_me/tooling.hpp>
#include <llvm/ADT/StringRef.h>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/permutation.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"

namespace {
void logLoc(auto Loc) {
  INFO(fmt::format("Test location ({}:{}:{})", Loc.file_name(), Loc.line(),
                   Loc.column()));
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
  INFO(fmt::format("Expected:\t{}\nFound:\t{}\nNot found:\t{}\nNot "
                   "expected:\t{}",
                   ExpectedPaths, FoundPathsAsString,
                   ToSetDifference(ExpectedPaths, FoundPathsAsString),
                   ToSetDifference(FoundPathsAsString, ExpectedPaths)));
  REQUIRE(ExpectedEqualityResult ==
          ranges::equal(FoundPathsAsString, ExpectedPaths));
}
} // namespace

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
  logLoc(Loc);
  const auto [AST, Transitions] = collectTransitions(Code, CurrentConfig);
  const auto FoundPathsAsString =
      buildGraphAndFindPaths(Transitions, QueriedType, CurrentConfig);
  verify(true, FoundPathsAsString, ExpectedPaths);
}

void testFailure(const std::string_view Code,
                 const std::string_view QueriedType,
                 const std::set<std::string, std::less<>> &ExpectedPaths,
                 const Config &CurrentConfig, const std::source_location Loc) {
  logLoc(Loc);
  const auto [AST, Transitions] = collectTransitions(Code, CurrentConfig);
  const auto FoundPathsAsString =
      buildGraphAndFindPaths(Transitions, QueriedType, CurrentConfig);
  verify(false, FoundPathsAsString, ExpectedPaths);
}

void testNoThrow(const std::string_view Code,
                 const std::string_view QueriedType,
                 const Config &CurrentConfig, const std::source_location Loc) {
  const auto Test = [&Loc, &Code, &QueriedType, &CurrentConfig]() {
    logLoc(Loc);
    const auto [AST, Transitions] = collectTransitions(Code, CurrentConfig);
    std::ignore =
        buildGraphAndFindPaths(Transitions, QueriedType, CurrentConfig);
  };

  REQUIRE_NOTHROW(Test());
}

void testQueryAll(const std::string_view Code, const Config &CurrentConfig,
                  const std::source_location Loc) {
  logLoc(Loc);

  const auto [AST, Transitions] = collectTransitions(Code, CurrentConfig);

  const auto Run = [&Transitions, &CurrentConfig](const auto &QueriedType) {
    const auto RunImpl = [&Transitions, &CurrentConfig,
                          QueriedType = fmt::format("{}", QueriedType)]() {
      std::ignore =
          buildGraphAndFindPaths(Transitions, QueriedType, CurrentConfig);
    };

    REQUIRE_NOTHROW(RunImpl());
  };

  ranges::for_each(*Transitions |
                       ranges::views::filter(ranges::empty, Element<1>) |
                       ranges::views::transform(ToAcquired),
                   Run);
}

std::pair<std::unique_ptr<clang::ASTUnit>, std::shared_ptr<TransitionCollector>>
collectTransitions(const std::string_view Code, const Config &CurrentConfig) {
  auto AST = clang::tooling::buildASTFromCodeWithArgs(Code, {"-std=c++20"});
  auto Transitions = std::make_shared<TransitionCollector>();
  auto Consumer = GetMe{CurrentConfig, *Transitions, AST->getSema()};
  Consumer.HandleTranslationUnit(AST->getASTContext());
  return {std::move(AST), std::move(Transitions)};
}

std::set<std::string>
buildGraphAndFindPaths(const std::shared_ptr<TransitionCollector> &Transitions,
                       const std::string_view QueriedType,
                       const Config &CurrentConfig) {
  const auto Query =
      QueryType{Transitions, std::string{QueriedType}, CurrentConfig};
  const auto [Graph, Data] = createGraph(Query);
  const auto SourceVertex =
      getSourceVertexMatchingQueriedType(Data, Query.getQueriedType());
  const auto VertexDataSize = Data.VertexData.size();
  REQUIRE(VertexDataSize != 0);
  REQUIRE(SourceVertex < VertexDataSize);

  // return instead of requires because querying all might query a type with no
  // edges/transitions that acquire it
  // FIXME: TransitionCollector should not contain entries with an empty set of
  // transitions for an acquired type
  if (Data.Edges.empty()) {
    return {};
  }

  const auto FoundPaths =
      pathTraversal(Graph, Data, CurrentConfig, SourceVertex);

  return toString(FoundPaths, Graph, Data) | ranges::to<std::set>;
}

#ifndef get_me_get_me_tests_hpp
#define get_me_get_me_tests_hpp

#include <functional>
#include <memory>
#include <set>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include <clang/Frontend/ASTUnit.h>
#include <get_me/config.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/indices.hpp>

#include "get_me/path_traversal.hpp"
#include "get_me/transitions.hpp"

void testSuccess(std::string_view Code, std::string_view QueriedType,
                 const std::set<std::string, std::less<>> &ExpectedPaths,
                 const Config &CurrentConfig = {},
                 std::source_location Loc = std::source_location::current());

void testFailure(std::string_view Code, std::string_view QueriedType,
                 const std::set<std::string, std::less<>> &ExpectedPaths,
                 const Config &CurrentConfig = {},
                 std::source_location Loc = std::source_location::current());

void testNoThrow(std::string_view Code, std::string_view QueriedType,
                 const Config &CurrentConfig = {},
                 std::source_location Loc = std::source_location::current());

void testQueryAll(std::string_view Code, const Config &CurrentConfig = {},
                  std::source_location Loc = std::source_location::current());

void test(std::string_view Code, std::string_view QueriedType,
          const std::set<std::string, std::less<>> &ExpectedPaths,
          const Config &CurrentConfig = {},
          std::source_location Loc = std::source_location::current());

void test(const auto &Generator, const size_t Count,
          const Config &CurrentConfig = {},
          std::source_location Loc = std::source_location::current()) {
  ranges::for_each(
      ranges::views::indices(size_t{1U}, Count),
      [&Generator, &Loc, &CurrentConfig](const auto NumRepetitions) {
        const auto &[Query, Code] = Generator(NumRepetitions);
        testNoThrow(Code, Query, CurrentConfig, Loc);
        testQueryAll(Code, CurrentConfig, Loc);
      });
}

[[nodiscard]] std::pair<std::unique_ptr<clang::ASTUnit>,
                        std::shared_ptr<TransitionCollector>>
collectTransitions(std::string_view Code, const Config &CurrentConfig = {});

[[nodiscard]] std::set<std::string>
buildGraphAndFindPaths(const TransitionCollector &Transitions,
                       std::string_view QueriedType,
                       const Config &CurrentConfig);

#endif

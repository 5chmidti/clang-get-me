#ifndef get_me_get_me_tests_hpp
#define get_me_get_me_tests_hpp

#include <cstddef>
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

#include "get_me/transitions.hpp"

using ResultPaths = std::set<std::string, std::less<>>;

void testSuccess(std::string_view Code, std::string_view QueriedType,
                 const ResultPaths &ExpectedPaths,
                 std::shared_ptr<Config> Conf = std::make_shared<Config>(),
                 std::source_location Loc = std::source_location::current());

void testFailure(std::string_view Code, std::string_view QueriedType,
                 const ResultPaths &ExpectedPaths,
                 std::shared_ptr<Config> Conf = std::make_shared<Config>(),
                 std::source_location Loc = std::source_location::current());

void testFailure(std::string_view Code, std::string_view QueriedType,
                 const ResultPaths &CurrentExpectedPaths,
                 const ResultPaths &ExpectedPaths,
                 std::shared_ptr<Config> Conf = std::make_shared<Config>(),
                 std::source_location Loc = std::source_location::current());

void testNoThrow(std::string_view Code, std::string_view QueriedType,
                 std::shared_ptr<Config> Conf = std::make_shared<Config>(),
                 std::source_location Loc = std::source_location::current());

void testQueryAll(std::string_view Code,
                  std::shared_ptr<Config> Conf = std::make_shared<Config>(),
                  std::source_location Loc = std::source_location::current());

void test(std::string_view Code, std::string_view QueriedType,
          const ResultPaths &ExpectedPaths,
          std::shared_ptr<Config> Conf = std::make_shared<Config>(),
          std::source_location Loc = std::source_location::current());

void test(const auto &Generator, const size_t Count,
          std::shared_ptr<Config> Conf = std::make_shared<Config>(),
          std::source_location Loc = std::source_location::current()) {
  ranges::for_each(ranges::views::indices(size_t{1U}, Count),
                   [&Generator, &Loc, &Conf](const auto NumRepetitions) {
                     const auto &[Query, Code] = Generator(NumRepetitions);
                     testNoThrow(Code, Query, Conf, Loc);
                     testQueryAll(Code, Conf, Loc);
                   });
}

[[nodiscard]] std::pair<std::unique_ptr<clang::ASTUnit>,
                        std::shared_ptr<TransitionData>>
collectTransitions(std::string_view Code, std::shared_ptr<Config> Conf = {});

[[nodiscard]] ResultPaths
buildGraphAndFindPaths(const std::shared_ptr<TransitionData> &Transitions,
                       std::string_view QueriedType,
                       std::shared_ptr<Config> Conf);

#endif

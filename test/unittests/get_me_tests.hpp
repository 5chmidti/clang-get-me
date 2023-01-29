#ifndef get_me_get_me_tests_hpp
#define get_me_get_me_tests_hpp

#include <set>
#include <source_location>
#include <string>
#include <string_view>

#include <get_me/config.hpp>
#include <gtest/gtest.h>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/indices.hpp>

void test(std::string_view Code, std::string_view QueriedType,
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

void testGenerator(const auto &Generator, const size_t Count,
                   const Config &CurrentConfig = {},
                   std::source_location Loc = std::source_location::current()) {
  ranges::for_each(
      ranges::views::indices(size_t{1U}, Count),
      [&Generator, &Loc, &CurrentConfig](const auto NumRepetitions) {
        const auto &[Query, Code] = Generator(NumRepetitions);
        testNoThrow(Code, Query, CurrentConfig, Loc);
      });
}

class GetMeTest : public testing::Test {
protected:
  GetMeTest();
  void TearDown() override;
};

#endif

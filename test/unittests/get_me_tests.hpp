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

class GetMeTest : public testing::Test {
public:
  void test(std::string_view Code, std::string_view QueriedType,
            const std::set<std::string, std::less<>> &ExpectedPaths,
            std::source_location Loc = std::source_location::current()) const;

  void
  testFailure(std::string_view Code, std::string_view QueriedType,
              const std::set<std::string, std::less<>> &ExpectedPaths,
              std::source_location Loc = std::source_location::current()) const;

  void
  testNoThrow(std::string_view Code, std::string_view QueriedType,
              std::source_location Loc = std::source_location::current()) const;

  void testGenerator(
      const auto &Generator, const size_t Count,
      std::source_location Loc = std::source_location::current()) const {
    ranges::for_each(ranges::views::indices(size_t{1U}, Count),
                     [&Generator, &Loc, this](const auto NumRepetitions) {
                       const auto &[Query, Code] = Generator(NumRepetitions);
                       testNoThrow(Code, Query, Loc);
                     });
  }

protected:
  GetMeTest();
  void SetUp() override;
  void TearDown() override;

  void setConfig(const Config &NewConfig) { CurrentConfig_ = NewConfig; }

private:
  Config CurrentConfig_;
  Config Config_;
};

#endif

#ifndef get_me_get_me_tests_hpp
#define get_me_get_me_tests_hpp

#include <set>
#include <source_location>
#include <string>
#include <string_view>

#include <get_me/config.hpp>
#include <gtest/gtest.h>

void test(std::string_view Code, std::string_view QueriedType,
          const std::set<std::string, std::less<>> &ExpectedPaths,
          const Config &CurrentConfig = {},
          std::source_location Loc = std::source_location::current());

void testFailure(std::string_view Code, std::string_view QueriedType,
                 const std::set<std::string, std::less<>> &ExpectedPaths,
                 const Config &CurrentConfig = {},
                 std::source_location Loc = std::source_location::current());

class GetMeTest : public testing::Test {
protected:
  GetMeTest();
  void TearDown() override;
};

#endif

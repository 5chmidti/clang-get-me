#ifndef get_me_get_me_tests_hpp
#define get_me_get_me_tests_hpp

#include <source_location>
#include <string>
#include <tuple>
#include <vector>

#include <get_me/graph.hpp>
#include <gtest/gtest.h>

[[nodiscard]] std::tuple<GraphData, GraphType, std::vector<PathType>>
prepare(std::string_view Code, std::string_view QueriedType);

void test(std::string_view Code, std::string_view QueriedType,
          std::vector<std::string_view> ExpectedPaths,
          std::source_location Loc = std::source_location::current());

class GetMeTest : public testing::Test {
protected:
  GetMeTest();
  void TearDown() override;
};

#endif

#include "get_me_tests.hpp"
#include "support/testcase_generation.hpp"

TEST(GetMeTest, generated_straight_path) {
  testGenerator(GenerateStraightPath, size_t{1} << size_t{6});
}

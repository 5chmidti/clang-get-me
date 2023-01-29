#include "get_me_tests.hpp"
#include "support/testcase_generation.hpp"

TEST_F(GetMeTest, generatedStraightPath) {
  test(GenerateStraightPath, size_t{1} << size_t{6});
}

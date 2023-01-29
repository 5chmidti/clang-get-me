#include "get_me_tests.hpp"
#include "support/testcase_generation.hpp"

TEST_F(GetMeTest, generatedForkingPath) {
  test(GenerateForkingPath, size_t{10});
}

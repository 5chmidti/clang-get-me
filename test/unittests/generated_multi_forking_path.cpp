#include "get_me_tests.hpp"
#include "support/testcase_generation.hpp"

TEST_F(GetMeTest, generatedMultiForkingPath) {
  test(GenerateMultiForkingPath, size_t{8});
}

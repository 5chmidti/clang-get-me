#include "get_me_tests.hpp"
#include "support/testcase_generation.hpp"

TEST(GetMeTest, generated_multi_forking_path) {
  testGenerator(GenerateMultiForkingPath, size_t{8});
}

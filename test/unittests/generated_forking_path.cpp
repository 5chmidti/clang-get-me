#include "get_me_tests.hpp"
#include "support/testcase_generation.hpp"

TEST(GetMeTest, generated_forking_path) {
  testGenerator(GenerateForkingPath, size_t{10});
}

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "get_me_tests.hpp"
#include "support/testcase_generation.hpp"

TEST_CASE("generated multi forking paths") {
  test(GenerateMultiForkingPath, size_t{6});
}

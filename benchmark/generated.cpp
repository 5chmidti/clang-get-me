#include <algorithm>
#include <cstddef>
#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

#include "get_me_benchmarks.hpp"
#include "support/testcase_generation.hpp"

// NOLINTNEXTLINE
GENERATE_GENERATED_BENCHMARKS(
    straightPath,
    GenerateStraightPath, ->Range(1, size_t{1U} << size_t{12U})->Complexity());

// NOLINTNEXTLINE
GENERATE_GENERATED_BENCHMARKS(
    forkingPath, GenerateForkingPath, ->DenseRange(1, 10, 1)->Complexity());

// NOLINTNEXTLINE
GENERATE_GENERATED_BENCHMARKS(
    templatePath,
    GenerateMultiForkingPath, ->DenseRange(1, 10, 1)->Complexity());

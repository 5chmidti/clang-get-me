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
GENERATE_GENERATED_BENCHMARKS(templatePath,
                              generateFromTemplate("A0", "struct A0 {};",
                                                   [](const size_t Iter) {
                                                     return fmt::format(
                                                         R"(
struct A{1};
struct B{0};
struct C{0};
struct D{0};
struct E{0};

A{0} getA{0}(B{0});
A{0} getA{0}(C{0});
B{0} getB{0}(D{0});
C{0} getC{0}(D{0});
C{0} getC{0}(E{0});
D{0} getD{0}(A{1});
E{0} getE{0}();
    )",
                                                         Iter, Iter + 1);
                                                   }),
                                  ->DenseRange(1, 10, 1)
                                  ->Complexity());

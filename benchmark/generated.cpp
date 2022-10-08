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
    generateFromTemplate("A0", "struct A0 {};",
                         [](const size_t Iter) -> std::string {
                           return fmt::format(R"(
struct D{1} {{
  A{0} &getA{0}();
}};

struct C{1} {{
  C{1}() = delete;
  static C{1} create();
}};

struct B{1} {{
  B{1}(A{0} &, C{1});
}};

struct A{1}{{ explicit A{1}(B{1}); }};

A{1} getA{1}();
B{1} getB{1}(int);
    )",
                                              Iter, Iter + 1);
                         }),
        ->Range(1, size_t{1U} << size_t{10U})
        ->Complexity());

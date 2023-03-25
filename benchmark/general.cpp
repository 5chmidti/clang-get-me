#include "get_me_benchmarks.hpp"

// NOLINTNEXTLINE
GENERATE_BENCHMARKS(baseline, "struct A {};", "A");

// NOLINTNEXTLINE
GENERATE_BENCHMARKS(simple,
                    R"(
struct E {
  E() = delete;
};

struct D {
  E &getE();
};

struct C {
  C() = delete;
  static C create();
};

struct B {
  B(E &, C);
};

struct A {
  explicit A(B);
};

A getA();
B getB(int);
B getB(float);
B getB(float, int);
)",
                    "A");

// NOLINTNEXTLINE
GENERATE_BENCHMARKS(std_string, "#include <string>", "std::string");
// NOLINTNEXTLINE
GENERATE_BENCHMARKS(std_stop_source, "#include <thread>", "std::stop_source");
// NOLINTNEXTLINE
GENERATE_BENCHMARKS(std_ostream, "#include <ostream>", "std::ostream");

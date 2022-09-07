#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <get_me/formatting.hpp>
#include <get_me/graph.hpp>
#include <gtest/gtest.h>
#include <range/v3/algorithm/permutation.hpp>
#include <spdlog/spdlog.h>

#include "get_me_tests.hpp"

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, simpleSingleEdge) {
  test(R"(
struct A {};

A getA();
A getA(int);
)",
       "A",
       {"({struct A}, A getA(), {})", "({struct A}, A getA(int), {int})",
        "({struct A}, A A(), {})"});

  test(R"(
struct A {};
struct B {};
A getA();
B getB();
)",
       "A", {"({struct A}, A getA(), {})", "({struct A}, A A(), {})"});

  test(R"(
struct A {};
struct B { static A StaticMemberA; };
)",
       "A", {"({struct A}, A StaticMemberA(), {})", "({struct A}, A A(), {})"});
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, simple) {
  test(R"(
struct A {};
struct B { A MemberA; };

A getA();
A getA(int);
B getB();
)",
       "A",
       {"({struct A}, A getA(), {})", "({struct A}, A A(), {})",
        "({struct A}, A getA(int), {int})",
        "({struct A}, A MemberA(B), {struct B}), ({struct B}, B getB(), {})",
        "({struct A}, A MemberA(B), {struct B}), ({struct B}, B B(), {})"});
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, overloads) {
  test(R"(
struct A {};

A getA();
A getA(int);
A getA(float, int);
A getA(float);
A getA(float, float);
)",
       "A",
       {"({struct A}, A getA(), {})", "({struct A}, A A(), {})",
        "({struct A}, A getA(int), {int})",
        "({struct A}, A getA(float), {float})",
        "({struct A}, A getA(float, float), {float})",
        "({struct A}, A getA(float, int), {int, float})"});

  test(R"(
struct A {};

A getA(float);
A getA(float, float);
)",
       "A",
       {"({struct A}, A getA(float, float), {float})",
        "({struct A}, A getA(float), {float})", "({struct A}, A A(), {})"});
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, specialMemberFunctions) {
  test(R"(
struct A {};
)",
       "A", {"({struct A}, A A(), {})"});

  test(R"(
struct A { A(); };
)",
       "A", {"({struct A}, A A(), {})"});

  test(R"(
struct A { explicit A(int); };
)",
       "A", {"({struct A}, A A(int), {int})"});

  test(R"(
struct A { A(int, float); };
)",
       "A", {"({struct A}, A A(int, float), {int, float})"});
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, inheritance) {
  test(R"(
struct A { A(int, float); };
struct B : public A {};
struct C : public B {};
A getA();
B getB();

)",
       "B",
       {"({struct B}, B getB(), {})",
        "({struct B}, A A(int, float), {int, float})",
        "({struct B}, B B(), {})"});
}

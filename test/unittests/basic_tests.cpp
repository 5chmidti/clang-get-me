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
       "A", {"A getA()", "A getA(int)", "A A()"});

  test(R"(
struct A {};
struct B {};
A getA();
B getB();
)",
       "A", {"A getA()", "A A()"});

  test(R"(
struct A {};
struct B { static A StaticMemberA; };
)",
       "A", {"A StaticMemberA()", "A A()"});
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
       {"A getA()", "A A()", "A getA(int)", "A MemberA(B), B getB()",
        "A MemberA(B), B B()"});
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
       {"A getA()", "A A()", "A getA(int)", "A getA(float)",
        "A getA(float, float)", "A getA(float, int)"});

  test(R"(
struct A {};

A getA(float);
A getA(float, float);
)",
       "A", {"A getA(float, float)", "A getA(float)", "A A()"});
}

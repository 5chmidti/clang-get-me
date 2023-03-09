#include <catch2/catch_test_macros.hpp>

#include "get_me_tests.hpp"

static constexpr auto PropagateTypeAliasConfig =
    Config{.EnablePropagateTypeAlias = true};

TEST_CASE("propagate type aliasing") {
  test(R"(
  struct A {};
  using B = A;
  A getA();
  B getB();
  )",
       "B", {"(B, B getB(), {})", "(B, A getA(), {})", "(B, A A(), {})"},
       PropagateTypeAliasConfig);

  test(R"(
  struct A {};
  using B = A;
  A getA();
  B getB();
  )",
       "A", {"(A, B getB(), {})", "(A, A getA(), {})", "(A, A A(), {})"},
       PropagateTypeAliasConfig);

  test(R"(
  template <typename T>
  struct A {
    explicit A(int);
  };
  using B = A<int>;
  A<double> a{42};
  B getB();
  B b = getB();
  )",
       "B", {"(B, A A(int), {int})", "(B, B getB(), {})"},
       PropagateTypeAliasConfig);

  test(R"(
  struct A {};
  using B = A;
  struct C {};
  C getC(A);
  void foo(B); // force creation of type alias
  )",
       "C",
       {
           "(C, C C(), {})",
           "(C, C getC(A), {A}), (A, A A(), {})",
           "(C, C getC(A), {B}), (B, A A(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
  struct A {};
  using B = A;
  struct C {};
  C getC(B);
  )",
       "C",
       {
           "(C, C C(), {})",
           "(C, C getC(B), {A}), (A, A A(), {})",
           "(C, C getC(B), {B}), (B, A A(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
  struct A {};
  using B = A;
  using C = A;
  using D = B;
  B getB();
  )",
       "B",
       {
           "(B, A A(), {})",
           "(B, B getB(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
  struct A {};
  using B = A;
  using C = A;
  using D = B;
  B getB();
  )",
       "C",
       {
           "(C, A A(), {})",
           "(C, B getB(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
  struct A {};
  using B = A;
  using D = A;
  using E = D;
  struct C {};
  C getC(B);
  )",
       "C",
       {
           "(C, C C(), {})",
           "(C, C getC(B), {A}), (A, A A(), {})",
           "(C, C getC(B), {B}), (B, A A(), {})",
           "(C, C getC(B), {D}), (D, A A(), {})",
           "(C, C getC(B), {E}), (E, A A(), {})",
       },
       PropagateTypeAliasConfig);
}

TEST_CASE("propagate type aliasing with member aliases") {
  test(R"(
       struct A {};
       struct B {
            using Other = A;
       };
    )",
       "A", {"(A, A A(), {})"}, PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       struct B {
            using Other = A;
       };
    )",
       "B::Other", {"(B::Other, A A(), {})"}, PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       struct B : public A {
            using Base = A;
       };
    )",
       "A", {"(A, A A(), {})", "(A, B B(), {})"}, PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       struct B : public A {
            using Base = A;
       };
    )",
       "B::Base", {"(B::Base, A A(), {})", "(B::Base, B B(), {})"},
       PropagateTypeAliasConfig);

  testFailure(R"(
       template <typename T>
       struct A {};
       struct B {
            using Other = A<int>;
       };
    )",
              "A<int>", {"(A, A A(), {})"}, PropagateTypeAliasConfig);

  testFailure(R"(
       template <typename T>
       struct A {};
       struct B {
            using Other = A<int>;
       };
    )",
              "B::Other", {"(B::Other, A A(), {})"}, PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       struct B : public A {
            using Base = A;
       };
    )",
       "A", {"(A, A A(), {})", "(A, B B(), {})"}, PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       struct B : public A {
            using Base = A;
       };
    )",
       "B::Base", {"(B::Base, A A(), {})", "(B::Base, B B(), {})"},
       PropagateTypeAliasConfig);
}

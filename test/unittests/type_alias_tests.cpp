#include "get_me_tests.hpp"

constexpr auto ConfigWithTypealiasPropagation =
    Config{.EnablePropagateTypeAlias = true};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, alias) {

  test(R"(
  struct A {};
  using B = A;
  A getA();
  B getB();
  )",
       "B", {"(B, B getB(), {})", "(B, A getA(), {})", "(B, A A(), {})"},
       ConfigWithTypealiasPropagation);

  test(R"(
  struct A {};
  using B = A;
  A getA();
  B getB();
  )",
       "A", {"(A, B getB(), {})", "(A, A getA(), {})", "(A, A A(), {})"},
       ConfigWithTypealiasPropagation);

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
       ConfigWithTypealiasPropagation);

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
       ConfigWithTypealiasPropagation);

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
       ConfigWithTypealiasPropagation);
}

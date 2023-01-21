#include "get_me/config.hpp"
#include "get_me_tests.hpp"

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, simpleSingleEdge) {
  test(R"(
struct A {};
)",
       "A", {"(A, A A(), {})"});

  test(R"(
struct A {};

A getA();
A getA(int);
)",
       "A", {"(A, A getA(), {})", "(A, A getA(int), {int})", "(A, A A(), {})"});

  test(R"(
struct A {};
struct B {};
A getA();
B getB();
)",
       "A", {"(A, A getA(), {})", "(A, A A(), {})"});

  test(R"(
struct A {};
struct B { static A StaticMemberA; };
)",
       "A", {"(A, A StaticMemberA(), {})", "(A, A A(), {})"});
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
       {"(A, A getA(), {})", "(A, A A(), {})", "(A, A getA(int), {int})",
        "(A, A MemberA(B), {B}), (B, B getB(), {})",
        "(A, A MemberA(B), {B}), (B, B B(), {})"});
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, overloads) {
  const auto ConfigWithOverloadFilter = Config{.EnableFilterOverloads = true};

  test(R"(
struct A {};

A getA();
A getA(int);
A getA(float, int);
A getA(float);
A getA(float, float);
)",
       "A",
       {"(A, A getA(), {})", "(A, A A(), {})", "(A, A getA(int), {int})",
        "(A, A getA(float), {float})", "(A, A getA(float, int), {int, float})"},
       ConfigWithOverloadFilter);

  test(R"(
struct A {};

A getA(float);
A getA(float, float);
)",
       "A", {"(A, A getA(float), {float})", "(A, A A(), {})"},
       ConfigWithOverloadFilter);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, specialMemberFunctions) {
  test(R"(
struct A {};
)",
       "A", {"(A, A A(), {})"});

  test(R"(
struct A { A(); };
)",
       "A", {"(A, A A(), {})"});

  test(R"(
struct A { explicit A(int); };
)",
       "A", {"(A, A A(int), {int})"});

  test(R"(
struct A { A(int, float); };
)",
       "A", {"(A, A A(int, float), {int, float})"});

  test(R"(
struct A {
  A() = default;
  explicit A(int);
  A(int, float);
};
)",
       "A",
       {"(A, A A(), {})", "(A, A A(int), {int})",
        "(A, A A(int, float), {int, float})"});

  test(R"(
struct A {
  A();
  virtual void foo() = 0;
};
A& getA();
)",
       "A &", {"(A &, A & getA(), {})"});

  test(R"(
struct A {
  A();
  virtual void foo() = 0;
};
struct B : public A {
  void foo();
};
)",
       "B", {"(B, B B(), {})"});
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, inheritance) {
  const auto ConfigWithInheritancePropagation =
      Config{.EnablePropagateInheritance = true};

  test(R"(
  struct A { A(int, float); };
  struct B : public A {};
  struct C : public B {};
  A getA();
  B getB();
  )",
       "B", {"(B, B getB(), {})", "(B, A A(int, float), {int, float})"},
       ConfigWithInheritancePropagation);

  test(R"(
struct A { A(int, float); };
struct B : public A {};
struct C : public B {};
A getA();
B getB();
)",
       "A",
       {"(A, A getA(), {})", "(A, B getB(), {})",
        "(A, A A(int, float), {int, float})"},
       ConfigWithInheritancePropagation);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, typealias) {
  const auto ConfigWithTypealiasPropagation =
      Config{.EnablePropagateTypeAlias = true};

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
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, templates) {
  test(R"(
  template <typename T>
  struct A {};
  A<int> getA();
  )",
       "A<int>", {"(A<int>, A<int> getA(), {})"});
}

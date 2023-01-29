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
  setConfig(Config{.EnableFilterOverloads = true});

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
        "(A, A getA(float), {float})",
        "(A, A getA(float, int), {int, float})"});

  test(R"(
struct A {};

A getA(float);
A getA(float, float);
)",
       "A", {"(A, A getA(float), {float})", "(A, A A(), {})"});
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
TEST_F(GetMeTest, templates) {
  test(R"(
  template <typename T>
  struct A {};
  A<int> getA();
  )",
       "A<int>", {"(A<int>, A<int> getA(), {})"});
}
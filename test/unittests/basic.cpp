#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "get_me/config.hpp"
#include "get_me_tests.hpp"

TEST_CASE("simple single edges") {
  test(R"(
struct A {};
)",
       "A",
       {
           "(A, A A(), {})",
       });

  test(R"(
struct A {};

A getA();
A getA(int);
)",
       "A",
       {
           "(A, A getA(), {})",
           "(A, A getA(int), {arithmetic})",
           "(A, A A(), {})",
       });

  test(R"(
struct A {};
struct B {};
A getA();
B getB();
)",
       "A",
       {
           "(A, A getA(), {})",
           "(A, A A(), {})",
       });

  test(R"(
struct A {};
struct B { static A StaticMemberA; };
)",
       "A",
       {
           "(A, A StaticMemberA(), {})",
           "(A, A A(), {})",
       });
}

TEST_CASE("simple") {
  test(R"(
struct A {};
struct B { A MemberA; };

A getA();
A getA(int);
B getB();
)",
       "A",
       {
           "(A, A getA(), {})",
           "(A, A A(), {})",
           "(A, A getA(int), {arithmetic})",
           "(A, A MemberA(B), {B}), (B, B getB(), {})",
           "(A, A MemberA(B), {B}), (B, B B(), {})",
       });

  test(R"(
    struct A {};
    void foo() {
      A localA{};
    }
  )",
       "A",
       {
           "(A, A A(), {})",
       });

  testFailure(R"(
    struct A {};
    void foo() {
      struct B {};
    }
  )",
              "B", {});
}

TEST_CASE("special member functions") {
  test(R"(
struct A {};
)",
       "A",
       {
           "(A, A A(), {})",
       });

  test(R"(
struct A { A(); };
)",
       "A",
       {
           "(A, A A(), {})",
       });

  test(R"(
struct A { explicit A(int); };
)",
       "A",
       {
           "(A, A A(int), {arithmetic})",
       });

  test(R"(
struct A { A(int, float); };
)",
       "A",
       {
           "(A, A A(int, float), {arithmetic})",
       });

  test(R"(
struct A {
  A() = default;
  explicit A(int);
  A(int, float);
};
)",
       "A",
       {
           "(A, A A(), {})",
           "(A, A A(int), {arithmetic})",
           "(A, A A(int, float), {arithmetic})",
       });

  test(R"(
struct A {
  A();
  virtual void foo() = 0;
};
A& getA();
)",
       "A &",
       {
           "(A &, A & getA(), {})",
       });

  test(R"(
struct A {
  A();
  virtual void foo() = 0;
};
struct B : public A {
  void foo();
};
)",
       "B",
       {
           "(B, B B(), {})",
       });
}

TEST_CASE("templates") {
  testFailure(R"(
  struct B {};
  template <typename T>
  struct A {
    A() = default;
    B getB();
  };
  A<int> getA();
  )",
              "A<int>",
              {
                  "(A<int>, A<int> getA(), {})",
              },
              {
                  "(A<int>, A A(), {})",
                  "(A<int>, A<int> getA(), {})",
              });

  testFailure(R"(
  template <typename T>
  struct A { A() = default; };
  A<int> getA();
  )",
              "A<int>",
              {
                  "(A<int>, A<int> getA(), {})",
              },
              {
                  "(A<int>, A A(), {})",
                  "(A<int>, A<int> getA(), {})",
              });
}

TEST_CASE("arithmetic") {
  test(R"(
    struct A { A(int); };
    int getInt();
  )",
       "A",
       {
           "(A, A A(int), {arithmetic})",
       });

  test(R"(
    struct A { A(int); };
    float getFloat();
  )",
       "A",
       {
           "(A, A A(int), {arithmetic})",
       });
}

TEST_CASE("back edges") {
  test(R"(
    struct A { A() = delete; };
    struct B { B() = delete; };
    struct C { C() = delete; };
    struct D { D() = delete; };

    D getD(A);
    D getD(C);
    C getC(B);
    B getB(A);
    A getA();
  )",
       "D",
       {
           "(D, D getD(A), {A}), (A, A getA(), {})",
           "(D, D getD(C), {C}), (C, C getC(B), {B}), (B, B getB(A), {A}), (A, "
           "A getA(), {})",
       },
       std::make_shared<Config>(Config{.EnableGraphBackwardsEdge = true}));
  test(R"(
    struct A { A() = delete; };
    struct B { B() = delete; };
    struct C { C() = delete; };
    struct D { D() = delete; };

    D getD(A);
    D getD(C);
    C getC(B);
    B getB(A);
    A getA();
  )",
       "D",
       {
           "(D, D getD(A), {A}), (A, A getA(), {})",
           "(D, D getD(C), {C}), (C, C getC(B), {B})",
       },
       std::make_shared<Config>(Config{.EnableGraphBackwardsEdge = false}));
}

TEST_CASE("dependent") {
  test(R"(
        struct C {};
        struct B { B(C); };
        struct A { A(B, C); };
        )",
       "A",
       {
           "(A, A A(B, C), {C, B}), (B, B B(C), {C}), (C, C C(), {})",
       });
}

TEST_CASE("cycles") {
  test(R"(
        struct A {};
        using B = A;
        A getA(B);
        )",
       "A",
       {
           "(A, A A(), {})",
       });

  test(R"(
        struct A {};
        A getA(const A);
        const A getA2(A);
        )",
       "A",
       {
           "(A, A A(), {})",
       });
}

#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "get_me/config.hpp"
#include "get_me_tests.hpp"

static const auto PropagateInheritanceConfig =
    std::make_shared<Config>(Config{.EnablePropagateInheritance = true});

TEST_CASE("propagate inheritance") {
  test(R"(
    struct A {};
    struct B : public A {};

    A getA();
  )",
       "B",
       {
           "(B, B B(), {})",
       },
       PropagateInheritanceConfig);

  test(R"(
    struct A {};
    struct B : public A {};

    A getA();
  )",
       "A",
       {
           "(A, A A(), {})",
           "(A, B B(), {})",
           "(A, A getA(), {})",
       },
       PropagateInheritanceConfig);

  test(R"(
    struct A {};
    struct B : private A {};

    A getA();
  )",
       "B",
       {
           "(B, B B(), {})",
       },
       PropagateInheritanceConfig);

  test(R"(
  struct A { A(int, float); };
  struct B : public A {};
  struct C : public B {};
  A getA();
  B getB();
  )",
       "B",
       {
           "(B, B getB(), {})",
       },
       PropagateInheritanceConfig);

  test(R"(
struct A { A(int, float); };
struct B : public A {};
struct C : public B {};
A getA();
B getB();
)",
       "A",
       {
           "(A, A getA(), {})",
           "(A, B getB(), {})",
           "(A, A A(int, float), {arithmetic})",
       },
       PropagateInheritanceConfig);

  testFailure(
      R"(
    struct A {};
    struct B : private A {};
    struct C {};

    C getC(A);
  )",
      "C",
      {
          "(C, C C(), {})",
          "(C, C getC(A), {A}), (A, A A(), {})",
          "(C, C getC(A), {B}), (B, B B(), {})",
          "(C, C getC(A), {A}), (A, B B(), {})",
      },
      {
          "(C, C C(), {})",
          "(C, C getC(A), {A}), (A, A A(), {})",
      },
      PropagateInheritanceConfig);
}

TEST_CASE("propagate inheritence: qualified types") {
  test(
      R"(
    struct A {};
    struct B : public A {};
    struct C {};
    struct D {};

    C funcA(A);
    C funcB(B);

    A funcA(D);
    B funcB(D);
  )",
      "C",
      {
          "(C, C C(), {})",
          "(C, C funcA(A), {A}), (A, A A(), {})",
          "(C, C funcA(A), {A}), (A, B B(), {})",
          "(C, C funcA(A), {B}), (B, B B(), {})",
          "(C, C funcB(B), {B}), (B, B B(), {})",
          "(C, C funcA(A), {A}), (A, A funcA(D), {D}), (D, D D(), {})",
          "(C, C funcA(A), {A}), (A, B funcB(D), {D}), (D, D D(), {})",
          "(C, C funcA(A), {B}), (B, B funcB(D), {D}), (D, D D(), {})",
          "(C, C funcB(B), {B}), (B, B funcB(D), {D}), (D, D D(), {})",
      },
      PropagateInheritanceConfig);

  testFailure(
      R"(
    struct A {};
    struct B : public A {};
    struct C {};
    struct D {};

    C funcA(A&);
    C funcB(B&);

    A& funcA(D);
    B& funcB(D);
  )",
      "C",
      {
          "(C, C C(), {})",
          "(C, C funcA(A &), {A &}), (A, A A(), {})",
          "(C, C funcA(A &), {A &}), (A, B B(), {})",
          "(C, C funcB(B &), {B &}), (B, B B(), {})",
          "(C, C funcA(A &), {A &}), (A &, A & funcA(D), {D}), (D, D D(), {})",
          "(C, C funcB(B &), {B &}), (B &, B & funcB(D), {D}), (D, D D(), {})",
      },
      {
          "(C, C C(), {})",
          "(C, C funcA(A &), {B &}), (B, B B(), {})",
          "(C, C funcA(A &), {A &}), (A &, A & funcA(D), {D}), (D, D D(), {})",
          "(C, C funcA(A &), {A &}), (A &, B & funcB(D), {D}), (D, D D(), {})",
          "(C, C funcA(A &), {B &}), (B &, B & funcB(D), {D}), (D, D D(), {})",
          "(C, C funcB(B &), {B &}), (B &, B & funcB(D), {D}), (D, D D(), {})",
      },
      PropagateInheritanceConfig);

  testFailure(
      R"(
    struct A {};
    struct B : public A {};
    struct C {};
    struct D {};

    C funcA(A*);
    C funcB(B*);

    A* funcA(D);
    B* funcB(D);
  )",
      "C",
      {
          "(C, C C(), {})",
          "(C, C funcA(A *), {A *}), (A, A A(), {})",
          "(C, C funcA(A *), {A *}), (A, B B(), {})",
          "(C, C funcB(B *), {B *}), (B, B B(), {})",
          "(C, C funcA(A *), {A *}), (A *, A * funcA(D), {D}), (D, D D(), {})",
          "(C, C funcB(B *), {B *}), (B *, B * funcB(D), {D}), (D, D D(), {})",
      },
      {
          "(C, C C(), {})",
          "(C, C funcA(A *), {B *}), (B, B B(), {})",
          "(C, C funcA(A *), {A *}), (A, A funcA(D), {D}), (D, D D(), {})",
          "(C, C funcA(A), {A}), (A, B * funcB(D), {D}), (D, D D(), {})",
          "(C, C funcA(A), {B}), (B, B * funcB(D), {D}), (D, D D(), {})",
          "(C, C funcB(B), {B}), (B, B * funcB(D), {D}), (D, D D(), {})",
          "(C, C funcA(A *), {A *}), (A *, A * funcA(D), {D}), (D, D D(), {})",
          "(C, C funcB(B *), {B *}), (B *, B * funcB(D), {D}), (D, D D(), {})",
      },
      PropagateInheritanceConfig);
}

TEST_CASE("propagate inheritence: qualified types virtual") {
  testFailure(
      R"(
    struct A { virtual void foo()=0; };
    struct B : public A { void foo() final; };

    B funcB(B);
  )",
      "B",
      {
          "(B, B B(), {})",
      },
      {}, PropagateInheritanceConfig);

  testFailure(
      R"(
    struct A { virtual void foo()=0; };
    struct B : public A { void foo() final; };

    A& funcARef(A&);
    B& funcBRef(B&);
  )",
      "B",
      {
          "(B, B B(), {})",
      },
      {}, PropagateInheritanceConfig);

  testFailure(
      R"(
    struct A { virtual void foo()=0; };
    struct B : public A { void foo() final; };

    A* funcAPtr(A*);
    B* funcBPtr(B*);
  )",
      "B",
      {
          "(B, B B(), {})",
      },
      {}, PropagateInheritanceConfig);
}

// FIXME: test with general operators as well
TEST_CASE("propagate inheritance: constructors") {
  test(R"(
        struct A {
            A();
        };
        struct B : public A {
            explicit B(int);
        };
    )",
       "B",
       {
           "(B, B B(int), {arithmetic})",
       },
       PropagateInheritanceConfig);

  test(R"(
        struct A {
            A();
        };
        struct B : public A {
            using A::A;
            explicit B(int);
        };
    )",
       "B",
       {
           "(B, B B(int), {arithmetic})",
           "(B, B B(), {})",
       },
       PropagateInheritanceConfig);

  test(R"(
        struct A {
            explicit A(int);
        };
        struct B : public A {
        };
    )",
       "B", {}, PropagateInheritanceConfig);

  test(R"(
        struct A {
            A();
        };
        struct B : public A {
            using A::A;
        };
    )",
       "B",
       {
           "(B, B B(), {})",
       },
       PropagateInheritanceConfig);

  testFailure(R"(
        struct A {
            explicit A(int);
        };
        struct B : public A {
            using A::A;
        };
    )",
              "B",
              {
                  "(B, A A(int), {arithmetic})",
              },
              PropagateInheritanceConfig);
  test(R"(
        struct A {
            explicit A(int);
        };
        struct B : public A {
            using A::A;
        };
        B b = B(42);
    )",
       "B",
       {
           "(B, B B(int), {arithmetic})",
       },
       PropagateInheritanceConfig);
}

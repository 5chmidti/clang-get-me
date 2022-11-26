#include "get_me_tests.hpp"

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, inheritance) {
  test(R"(
    struct A {};
    struct B : public A {};

    A getA();
  )",
       "B",
       {
           "(struct B, B B(), {})",
       });

  test(R"(
    struct A {};
    struct B : public A {};

    A getA();
  )",
       "A",
       {
           "(struct A, A A(), {})",
           "(struct A, B B(), {})",
           "(struct A, A getA(), {})",
       });

  test(R"(
    struct A {};
    struct B : private A {};

    A getA();
  )",
       "B",
       {
           "(struct B, B B(), {})",
       });

  testFailure(
      R"(
    struct A {};
    struct B : private A {};
    struct C {};

    C getC(A);
  )",
      "C",
      {
          "(struct C, C C(), {})",
          "(struct C, C getC(A), {struct A}), (struct A, A A(), {})",
          "(struct C, C getC(A), {struct B}), (struct B, B B(), {})",
      });
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, inheritenceQualifiedTypes) {
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
          "(struct C, C C(), {})",
          "(struct C, C funcA(A), {struct A}), (struct A, A A(), {})",
          "(struct C, C funcA(A), {struct A}), (struct A, B B(), {})",
          "(struct C, C funcA(A), {struct B}), (struct B, B B(), {})",
          "(struct C, C funcB(B), {struct B}), (struct B, B B(), {})",
          "(struct C, C funcA(A), {struct A}), (struct A, A funcA(D), "
          "{struct D}), (struct D, D D(), {})",
          "(struct C, C funcA(A), {struct A}), (struct A, B funcB(D), "
          "{struct D}), (struct D, D D(), {})",
          "(struct C, C funcA(A), {struct B}), (struct B, B funcB(D), "
          "{struct D}), (struct D, D D(), {})",
          "(struct C, C funcB(B), {struct B}), (struct B, B funcB(D), "
          "{struct D}), (struct D, D D(), {})",
      });

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
          "(struct C, C C(), {})",
          "(struct C, C funcA(A&), {struct A}), (struct A, A A(), {})",
          "(struct C, C funcA(A&), {struct A}), (struct A, B B(), {})",
          "(struct C, C funcA(A&), {struct B}), (struct B, B B(), {})",
          "(struct C, C funcB(B&), {struct B}), (struct B, B B(), {})",
          "(struct C, C funcA(A&), {struct A}), (struct A, A& funcA(D), "
          "{struct D}), (struct D, D D(), {})",
          "(struct C, C funcA(A&), {struct A}), (struct A, B& funcB(D), "
          "{struct D}), (struct D, D D(), {})",
          "(struct C, C funcA(A&), {struct B}), (struct B, B& funcB(D), "
          "{struct D}), (struct D, D D(), {})",
          "(struct C, C funcB(B&), {struct B}), (struct B, B& funcB(D), "
          "{struct D}), (struct D, D D(), {})",
      });

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
          "(struct C, C C(), {})",
          "(struct C, C funcA(A*), {struct A}), (struct A, A A(), {})",
          "(struct C, C funcA(A*), {struct A}), (struct A, B B(), {})",
          "(struct C, C funcA(A*), {struct B}), (struct B, B B(), {})",
          "(struct C, C funcB(B*), {struct B}), (struct B, B B(), {})",
          "(struct C, C funcA(A*), {struct A}), (struct A, A funcA(D), "
          "{struct D}), (struct D, D D(), {})",
          "(struct C, C funcA(A), {struct A}), (struct A, B* funcB(D), "
          "{struct D}), (struct D, D D(), {})",
          "(struct C, C funcA(A), {struct B}), (struct B, B* funcB(D), "
          "{struct D}), (struct D, D D(), {})",
          "(struct C, C funcB(B), {struct B}), (struct B, B* funcB(D), "
          "{struct D}), (struct D, D D(), {})",
      });
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-owning-memory)
TEST_F(GetMeTest, inheritenceQualifiedTypesVirtual) {
  testFailure(
      R"(
    struct A { virtual void foo()=0; };
    struct B : public A { void foo() final; };

    B funcB(B);
  )",
      "B",
      {
          "(struct B, B B(), {})",
          "(struct B, B funcB(B), {struct B}), (struct B, B B(), {})",
      });

  testFailure(
      R"(
    struct A { virtual void foo()=0; };
    struct B : public A { void foo() final; };

    A& funcARef(A&);
    B& funcBRef(B&);
  )",
      "B",
      {
          "(struct B, B B(), {})",
          "(struct B, B& funcBRef(B&), {struct B}), (struct B, B B(), {})",
      });

  testFailure(
      R"(
    struct A { virtual void foo()=0; };
    struct B : public A { void foo() final; };

    A* funcAPtr(A*);
    B* funcBPtr(B*);
  )",
      "B",
      {
          "(struct B, B B(), {})",
          "(struct B, B* funcBPtr(B*), {struct B}), (struct B, B B(), {})",
      });
}

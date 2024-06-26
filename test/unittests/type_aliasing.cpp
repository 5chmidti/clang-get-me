#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "get_me/config.hpp"
#include "get_me_tests.hpp"

static const auto PropagateTypeAliasConfig =
    std::make_shared<Config>(Config{.EnablePropagateTypeAlias = true});

TEST_CASE("propagate type aliasing") {
  test(R"(
  struct A {};
  using B = A;
  A getA();
  B getB();
  )",
       "B",
       {
           "(B, B getB(), {})",
           "(A, A getA(), {})",
           "(A, A A(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
  struct A {};
  using B = A;
  A getA();
  B getB();
  )",
       "A",
       {
           "(B, B getB(), {})",
           "(A, A getA(), {})",
           "(A, A A(), {})",
       },
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
       "B",
       {
           "(A<int>, A A(int), {arithmetic})",
           "(B, B getB(), {})",
       },
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
           "(C, C getC(B), {B}), (A, A A(), {})",
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
           "(A, A A(), {})",
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
           "(A, A A(), {})",
           "(B, B getB(), {})",
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
           "(C, C getC(B), {B}), (A, A A(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
  struct A1 {};
  using B1 = A1;
  using D1 = A1;
  using E1 = D1;
  struct C1 {};
  C1 getC(B1);

  struct A2 {};
  using B2 = A2;
  using D2 = A2;
  using E2 = D2;
  struct C2 {};
  C2 getC(B2);
  )",
       "C1",
       {
           "(C1, C1 C1(), {})",
           "(C1, C1 getC(B1), {B1}), (A1, A1 A1(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       template <typename T>
       struct B {};

       using A2 = A;
       using B2 = B<int>;
       template <typename T>
       using B3 = B<T>;
    )",
       "A",
       {
           "(A, A A(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       template <typename T>
       struct B {};

       using A2 = A;
       using B2 = B<int>;
       template <typename T>
       using B3 = B<T>;
    )",
       "A2",
       {
           "(A, A A(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
       struct A;
       struct B;
       using C = A;
       using D = C;
       using E = B;

       A getA();
       D getD();
    )",
       "A",
       {
           "(D, D getD(), {})",
           "(A, A getA(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
          using MyInt = int;
          using My2ndInt = int;

          MyInt getMyInt(My2ndInt);
     )",
       "int", {}, PropagateTypeAliasConfig);
}

TEST_CASE("propagate type aliasing with member aliases") {
  test(R"(
       struct A {};
       struct B {
            using Other = A;
       };
    )",
       "A",
       {
           "(A, A A(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       struct B {
            using Other = A;
       };
    )",
       "B::Other",
       {
           "(A, A A(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       struct B : public A {
            using Base = A;
       };
    )",
       "A",
       {
           "(A, A A(), {})",
           "(A, B B(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       struct B : public A {
            using Base = A;
       };
    )",
       "B::Base",
       {
           "(A, A A(), {})",
           "(A, B B(), {})",
       },
       PropagateTypeAliasConfig);

  testFailure(R"(
       template <typename T>
       struct A {};
       struct B {
            using Other = A<int>;
       };
    )",
              "A<int>", {},
              {
                  "(A, A A(), {})",
              },
              PropagateTypeAliasConfig);

  testFailure(R"(
       template <typename T>
       struct A {};
       struct B {
            using Other = A<int>;
       };
    )",
              "B::Other", {},
              {
                  "(B::Other, A A(), {})",
              },
              PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       struct B : public A {
            using Base = A;
       };
    )",
       "A",
       {
           "(A, A A(), {})",
           "(A, B B(), {})",
       },
       PropagateTypeAliasConfig);

  test(R"(
       struct A {};
       struct B : public A {
            using Base = A;
       };
    )",
       "B::Base",
       {
           "(A, A A(), {})",
           "(A, B B(), {})",
       },
       PropagateTypeAliasConfig);
}

TEST_CASE("propagate type aliasing with templates") {
  test(R"(
       struct A {};
       template <typename T>
       struct B {};

       using A2 = A;
       using B2 = B<int>;
       template <typename T>
       using B3 = B<T>;
    )",
       "B<T>",
       {
           "(B<T>, B B(), {})",
       },
       PropagateTypeAliasConfig);

  testFailure(R"(
       struct A {};
       template <typename T>
       struct B {};

       using A2 = A;
       using B2 = B<int>;
       template <typename T>
       using B3 = B<T>;
    )",
              "B2", {},
              {
                  "(B<int>, B B(), {})",
              },
              PropagateTypeAliasConfig);

  //   testFailure(R"(
  //        struct A {};
  //        template <typename T>
  //        struct B {};

  //        using A2 = A;
  //        using B2 = B<int>;
  //        template <typename T>
  //        using B3 = B<T>;
  //     )",
  //               "B3<T>", {},
  //               {
  //                   "(B<T>, B B(), {})",
  //               },
  //               PropagateTypeAliasConfig);
}

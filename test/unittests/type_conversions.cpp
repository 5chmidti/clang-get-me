#include <catch2/catch_test_macros.hpp>

#include "get_me_tests.hpp"

TEST_CASE("const propagation") {
  test(R"(
        struct A {};
        struct B { B(const A); };
        )",
       "B",
       {
           "(B, B B(const A), {const A}), (A, A A(), {})",
       });
}

TEST_CASE("ref propagation") {
  test(R"(
        struct A {};
        struct B { B(A &); };
        )",
       "B",
       {
           "(B, B B(A &), {A &}), (A, A A(), {})",
       });

  test(R"(
        struct A {};
        struct B { B(const A &); };
        )",
       "B",
       {
           "(B, B B(const A &), {const A &}), (A, A A(), {})",
       });

  // FIXME: require copyable type
  test(R"(
        struct A { A() = delete; };
        A& getARef();
        struct B { B(A); };
        )",
       "B",
       {
           "(B, B B(A), {A}), (A &, A & getARef(), {})",
       });

  test(R"(
        struct A { A() = delete; };
        A& getARef();
        struct B { B(const A); };
        )",
       "B",
       {
           "(B, B B(const A), {const A}), (A &, A & getARef(), {})",
       });

  test(R"(
        struct A { A() = delete; };
        A& getARef();
        struct B { B(const A&); };
        )",
       "B",
       {
           "(B, B B(const A &), {const A &}), (A &, A & getARef(), {})",
       });

  test(R"(
        struct A { A() = delete; };
        A& getARef();
        struct B { B(A); };
        )",
       "B",
       {
           "(B, B B(A), {A}), (A &, A & getARef(), {})",
       });
}

TEST_CASE("pointer propagation") {
  test(R"(
        struct A {};
        struct B { B(A *); };
        )",
       "B",
       {
           "(B, B B(A *), {A *}), (A, A A(), {})",
       });

  test(R"(
        struct A {};
        struct B { B(const A *); };
        )",
       "B",
       {
           "(B, B B(const A *), {const A *}), (A, A A(), {})",
       });

  test(R"(
        struct A { A() = delete;};
        A* getAPtr();
        struct B { B(A); };
        )",
       "B",
       {
           "(B, B B(A), {A}), (A *, A * getAPtr(), {})",
       });

  test(R"(
        struct A { A() = delete;};
        A* getAPtr();
        struct B { B(const A *); };
        )",
       "B",
       {
           "(B, B B(const A *), {const A *}), (A *, A * getAPtr(), {})",
       });

  test(R"(
        struct A { A() = delete;};
        A* getAPtr();
        struct B { B(const A); };
        )",
       "B",
       {
           "(B, B B(const A), {const A}), (A *, A * getAPtr(), {})",
       });
}

TEST_CASE("complex conversions") {
  test(R"(
        struct A {};
        struct B { B(A *); };
        )",
       "B",
       {
           "(B, B B(A *), {A *}), (A, A A(), {})",
       });

  test(R"(
        struct A {};
        struct B {
          B(const A *);
          B(const A &);
          B(A &);
          B(A *);
        };
        )",
       "B",
       {
           "(B, B B(const A *), {const A *}), (A, A A(), {})",
           "(B, B B(const A &), {const A &}), (A, A A(), {})",
           "(B, B B(A &), {A &}), (A, A A(), {})",
           "(B, B B(A *), {A *}), (A, A A(), {})",
       });
}

// struct A1 {};
// using B1 = A1;
// struct C1 {};
// C1 getC(B1);

// struct A2 {};
// using B2 = A2;
// struct C2 {};
// C2 getC(B2);

struct A {
  A() = delete;
};
struct B {};
struct C {};
struct D {};
D getA(B);
D getA(C);
A getA(D);

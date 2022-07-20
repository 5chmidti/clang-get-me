// RUN: get_me %s -t A -- | FileCheck %s

struct A {
  int X;
};

struct B {
  A Asdf{};
  // CHECK: :[[@LINE-1]]:5: note: found 'A' from source 'B'
};

struct C {
  [[nodiscard]] A foo();
  // CHECK: :[[@LINE-1]]:19: note: found 'A' from source 'foo'
};

[[nodiscard]] A foo();
// CHECK: :[[@LINE-1]]:17: note: found 'A' from source 'foo'

void bar() {
  [[maybe_unused]] A Asdf{};
  // CHECK: :[[@LINE-1]]:22: note: found 'A'
  [[maybe_unused]] int Qwerty{};
}

// RUN: get_me %s -t A -- | FileCheck %s

struct A {};

struct B {
  A Asdf{};
  // CHECK: :[[@LINE-1]]:5: note: found a source of 'A' within 'B'
};

struct C {
  [[nodiscard]] A foo();
  // CHECK: :[[@LINE-1]]:19: note: found a source of 'A'
};

[[nodiscard]] A foo();
// CHECK: :[[@LINE-1]]:17: note: found a source of 'A'

void bar() {
  [[maybe_unused]] A Asdf{};
  // CHECK: :[[@LINE-1]]:22: note: found a source of 'A'
  [[maybe_unused]] int Qwerty{};
}

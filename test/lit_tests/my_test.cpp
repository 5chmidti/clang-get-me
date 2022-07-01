// RUN: get_me %s -o qwerty -- | FileCheck %s

int main() {
  int MyVar = 3;
  // CHECK: :[[@LINE-1]]:15: note: my message
  return MyVar;
  // CHECK: :[[@LINE-1]]:10: note: my message
}

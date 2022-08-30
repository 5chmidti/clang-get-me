struct A {
  int MemberIntOfA;
};

struct B {
  A MemberAofB{};
};

struct C {
  [[nodiscard]] A getAofC();
};

[[nodiscard]] A getA();

void getNoneWithLocals() {
  [[maybe_unused]] const A LocalA{};
  [[maybe_unused]] const int LocalInt{};
}

[[nodiscard]] int getInt(float);
[[nodiscard]] float getFloat();

[[nodiscard]] A &getARef(C &Val);
[[nodiscard]] A getA(C Val);
[[nodiscard]] A getA(int Val);

[[nodiscard]] C getC(int Val);

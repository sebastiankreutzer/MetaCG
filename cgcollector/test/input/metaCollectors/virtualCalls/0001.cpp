

struct A {
  virtual void foo(){};
};

struct B: A {
  void foo() override {};
};

struct C: B {
  void foo() override {};
};

void testVirtual(A* a) {
  a->foo();
}

void testVirtual2(A* a) {
  for (int i = 0; i< 100; i++) {
    a->foo();
  }
}

void testDirect(C* a) {
  a->B::foo();
  a->C::foo();
}

void testBoth(C* a) {
  a->foo();
  a->C::foo();
}

int main(int argc, char **argv) {
  C* a = reinterpret_cast<C*>(argv[0]);
  testVirtual(a);
  testDirect(a);
  testBoth(a);
  return 0;
}

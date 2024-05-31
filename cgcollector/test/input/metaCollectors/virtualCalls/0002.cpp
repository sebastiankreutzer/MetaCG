

struct A {
  virtual void operator[](int){};
};

struct B: A {
  void operator[](int) override {};
};

struct C: B {
  void operator[](int)override {};
};

void testVirtual(A* a) {
  (*a)[0];
}

void testDirect(C* a) {
  a->B::operator[](0);
  a->C::operator[](0);
}

void testBoth(C* a) {
  (*a)[0];
  a->C::operator[](0);
}

void testDirect2() {
  C c;
  c[0];
}

int main(int argc, char **argv) {
  C* a = reinterpret_cast<C*>(argv[0]);
  testVirtual(a);
  testDirect(a);
  testBoth(a);
  return 0;
}

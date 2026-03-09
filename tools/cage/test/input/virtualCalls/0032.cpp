struct A {
  virtual int f() { return 1; }
};

struct B {
  virtual int g() { return 2; }
};

struct C : A, B {
  int f() override { return 3; }
  int g() override { return 4; }
};

int main() {
  C c;
  A* a = &c;
  B* b = &c;

  int r1 = a->f();
  int r2 = b->g();

  return 0;
}
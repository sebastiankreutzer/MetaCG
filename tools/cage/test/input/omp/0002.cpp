int main(int argc, char* argv[]) {
  int sum = 0;
  #pragma omp parallel
  #pragma omp single
  {
    for (int i = 0; i < 100; i++) {
      #pragma omp task
      sum += argc;
    }
  }
  return sum == 1 ? 1 : 0;
}

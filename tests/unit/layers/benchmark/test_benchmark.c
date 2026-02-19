#include "../../../../layers/benchmark/benchmark.h"
#include "../../../../layers/block_align/block_align.h"
#include "../../../../layers/local/local.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define num_reps 100000
#define block_size 4
#define TESTPATH "test_file.txt"
#define BENCHMARK_FILE "benchmark.txt"

LayerContext build_tree(int block_align) {
  LayerContext context_local = local_init();
  LayerContext context_block_align =
      block_align_init(&context_local, 1, block_size);
  LayerContext context_benchmark;
  if (block_align) {
    context_benchmark = benchmark_init(&context_block_align, 1, num_reps);
  } else {
    context_benchmark = benchmark_init(&context_local, 1, num_reps);
  }

  return context_benchmark;
}

void fill_file(int fd, int size) {
  char *buffer = malloc(size * sizeof(char));
  int i, j;

  for (i = 0; i < 5; i++) {
    char c = (char)(i + '0');
    for (j = 0; j < size; j++)
      buffer[j] = c;
    write(fd, buffer, size * sizeof(char));
  }

  free(buffer);
}

void test_benchmark_ops(LayerContext tree_BA, LayerContext tree_no_BA,
                        char *ops, int block_align) {

  int fd = open(TESTPATH, O_RDWR | O_TRUNC | O_CREAT, 0666);
  fill_file(fd, 4);

  if (strcmp(ops, "read") == 0) {
    char buffer[1000];
    if (block_align) {
      tree_BA.ops->lpread(fd, buffer, 6, 0, tree_BA);
    } else {
      tree_no_BA.ops->lpread(fd, buffer, 6, 0, tree_no_BA);
    }
  } else if (strcmp(ops, "write") == 0) {
    char buffer[3] = "999";
    if (block_align) {
      tree_BA.ops->lpwrite(fd, buffer, 3, 0, tree_BA);
    } else {
      tree_no_BA.ops->lpwrite(fd, buffer, 3, 0, tree_no_BA);
    }
  }

  close(fd);
}

void validate_times(double ba_time, double no_ba_time) {
  assert(ba_time > 0);
  assert(no_ba_time > 0);
}

void test_benchmark(LayerContext tree_BA, LayerContext tree_no_BA, char *ops) {
  printf("Testing the validation of a Benchmark %s example...\n", ops);
  int saved_stdout = dup(STDOUT_FILENO);

  int fd = open(BENCHMARK_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);
  dup2(fd, STDOUT_FILENO); // Redirect stdout

  test_benchmark_ops(tree_BA, tree_no_BA, ops, 1);
  (void)fflush(stdout);

  char tempo_BA[64] = {0};
  lseek(fd, 0, SEEK_SET);
  read(fd, tempo_BA, sizeof(tempo_BA));

  ftruncate(fd, 0); // limpar ficheiro
  lseek(fd, 0, SEEK_SET);

  test_benchmark_ops(tree_BA, tree_no_BA, ops, 0);
  (void)fflush(stdout);

  char tempo_no_BA[64] = {0};
  lseek(fd, 0, SEEK_SET);
  read(fd, tempo_no_BA, sizeof(tempo_no_BA));

  dup2(saved_stdout, STDOUT_FILENO);
  close(saved_stdout);
  close(fd);
  unlink(TESTPATH);
  unlink(BENCHMARK_FILE);

  double ba_time = 0;
  double no_ba_time = 0;

  char *start = strstr(tempo_BA, ":");
  if (!start) {
    (void)fprintf(stderr, "Malformed tempo_BA string\n");
    exit(EXIT_FAILURE);
  }
  ba_time = strtod(start + 1, NULL);

  start = strstr(tempo_no_BA, ":");
  if (!start) {
    (void)fprintf(stderr, "Malformed tempo_no_BA string\n");
    exit(EXIT_FAILURE);
  }
  no_ba_time = strtod(start + 1, NULL);

  validate_times(ba_time, no_ba_time);
  printf("✅ Test of the validation of a Benchmark %s example passed\n", ops);
}

int main(int argc, char const *argv[]) {
  printf("Running the Benchmark unit tests\n");
  LayerContext treeBA = build_tree(1);
  LayerContext tree_no_BA = build_tree(0);

  test_benchmark(treeBA, tree_no_BA, "read");
  test_benchmark(treeBA, tree_no_BA, "write");

  printf("🎉 All Benchmark unit tests passed!\n");
  return 0;
}

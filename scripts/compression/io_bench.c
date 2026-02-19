#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE (1 << 18) // 256 KiB

static double timespec_diff_sec(struct timespec a, struct timespec b) {
  return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}

static void kill(const char *msg) {
  perror(msg);
  exit(1);
}

static void do_write_only(const char *src, const char *dst, double *write_sec) {
  int in_fd = open(src, O_RDONLY);
  if (in_fd < 0)
    kill("open src");

  int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out_fd < 0)
    kill("open dst");

  char *buf = malloc(BUF_SIZE);
  if (!buf)
    kill("malloc");

  struct timespec t1, t2;
  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0)
    kill("clock_gettime");

  for (;;) {
    ssize_t r = read(in_fd, buf, BUF_SIZE);
    if (r < 0)
      kill("read");
    if (r == 0)
      break;
    char *p = buf;
    ssize_t left = r;
    while (left > 0) {
      ssize_t w = write(out_fd, p, left);
      if (w < 0)
        kill("write");
      left -= w;
      p += w;
    }
  }

  if (fsync(out_fd) != 0)
    kill("fsync");

  if (clock_gettime(CLOCK_MONOTONIC, &t2) != 0)
    kill("clock_gettime");

  if (write_sec)
    *write_sec = timespec_diff_sec(t1, t2);

  free(buf);
  close(in_fd);
  close(out_fd);
}

static void do_read_only(const char *path, double *read_sec) {
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    kill("open read");

  char *buf = malloc(BUF_SIZE);
  if (!buf)
    kill("malloc");

  struct timespec t1, t2;
  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0)
    kill("clock_gettime");

  for (;;) {
    ssize_t r = read(fd, buf, BUF_SIZE);
    if (r < 0)
      kill("read");
    if (r == 0)
      break;
  }

  if (clock_gettime(CLOCK_MONOTONIC, &t2) != 0)
    kill("clock_gettime");

  if (read_sec)
    *read_sec = timespec_diff_sec(t1, t2);

  free(buf);
  close(fd);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    (void)fprintf(stderr, "Usage: %s <mode> ...\n", argv[0]);
    (void)fprintf(stderr, "Modes:\n");
    (void)fprintf(stderr, "  write <src> <dst>\n");
    (void)fprintf(stderr, "  read <dst>\n");
    (void)fprintf(stderr, "  full <src> <dst>\n");
    return 1;
  }

  const char *mode = argv[1];

  if (strcmp(mode, "write") == 0) {
    if (argc != 4) {
      (void)fprintf(stderr, "write mode requires <src> <dst>\n");
      return 1;
    }
    double w = 0.0;
    do_write_only(argv[2], argv[3], &w);
    printf("WRITE_TIME_SEC:%.9f\n", w);
    return 0;
  } else if (strcmp(mode, "read") == 0) {
    if (argc != 3) {
      (void)fprintf(stderr, "read mode requires <dst>\n");
      return 1;
    }
    double r = 0.0;
    do_read_only(argv[2], &r);
    printf("READ_TIME_SEC:%.9f\n", r);
    return 0;
  } else if (strcmp(mode, "full") == 0) {
    if (argc != 4) {
      (void)fprintf(stderr, "full mode requires <src> <dst>\n");
      return 1;
    }
    double w = 0.0, r = 0.0;
    do_write_only(argv[2], argv[3], &w);
    do_read_only(argv[3], &r);
    printf("WRITE_TIME_SEC:%.9f\n", w);
    printf("READ_TIME_SEC:%.9f\n", r);
    return 0;
  } else {
    (void)fprintf(stderr, "Unknown mode: %s\n", mode);
    return 1;
  }
}

#ifndef __REMOTE_H__
#define __REMOTE_H__

#include "../../shared/types/layer_context.h"
#include "config.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define PORT 5000
#define LISTEN_BACKLOG 50
#define BSIZE 4096
#define PSIZE 512
#define PORT 5000
#define IP "127.0.0.1"

#define READ 0
#define WRITE 1
#define STAT 2
#define OPEN 3
#define UNLINK 4
#define CLOSE 5

typedef struct msg {
  int op;
  char path[PSIZE];
  char buffer[BSIZE];
  int flags;
  off_t offset;
  size_t size;
  ssize_t res;
  int fd;
  mode_t mode;
  struct stat st;
} MSG;

LayerContext remote_init();
ssize_t remote_pread(int fd, void *buffer, size_t nbyte, off_t offset,
                     LayerContext l);
ssize_t remote_pwrite(int fd, const void *buffer, size_t nbytes, off_t offset,
                      LayerContext l);

int remote_open(const char *pathname, int flags, mode_t mode, LayerContext l);
int remote_close(int fd, LayerContext l);

#endif // __REMOTE_H__

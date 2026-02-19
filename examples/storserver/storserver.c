#include "storserver.h"
#include <fcntl.h>
#include <glib.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPATH "/home/vagrant/server/"
// #define DEBUG "/home/vagrant/log"

int server_fd;
GHashTable *opened_files;

void intHandler(int dummy) {
  // NOLINTNEXTLINE(bugprone-signal-handler,cert-msc54-cpp)
  printf("Server shutting down\n");
  close(server_fd);
  // NOLINTNEXTLINE(bugprone-signal-handler,cert-msc54-cpp)
  exit(0);
}

// replaces path given by client (fuse) (e.g., /backend/file1.txt with
// /home/vagrant/server/file1.txt)
void handle_path(char *oldpath, char *newpath) {

  char *token, *string, *tofree;
  tofree = string = strdup(oldpath);
  int count = 0;

  while ((token = strsep(&string, "/")) != NULL) {
    if (count == 2) { // After the second '/'
      break;
    }
    count++;
  }

  strcpy(newpath, SPATH);
  if (token != NULL) {
    strcat(newpath, token); // SPATH+filename
  }

  free(tofree);
}

// Note: Create file if it doesn't exist
void handle_write(int socket_fd, MSG m) {
  ssize_t res;
  int fd;
  char path[PSIZE];

  // convert remote path to local path (SPATH+filename)
  handle_path(m.path, path);

  fd = open(path, O_CREAT | O_WRONLY, 0600);
  if (fd < 0) { // check for errors
    perror("open");
  }

  m.res = write(fd, &m.buffer, m.size);
  if (m.res < 0) { // check for errors
    perror("write");
  }

  // close file descriptor
  close(fd);

  // inform client that write was received
  strcpy(m.buffer, "write received");
  m.buffer[15] = '\0';
  res = write(socket_fd, &m, sizeof(m));
  if (res < 0)
    perror("write");

  printf("write for path: %s returned %ld\n", path, m.res);
}

void handle_read(int socket_fd, MSG m) {
  int fd;

  char path[PSIZE];
  char buff[m.size];
  handle_path(m.path, path);

  // open file to read from
  fd = open(path, O_RDONLY);
  if (fd < 0) { // checko for errors
    perror("open");
    m.res = 0;
  } else {
    m.res = read(fd, buff, m.size);
    if (m.res < 0) // check for errors
      perror("read");

    // send read content to client
    memcpy(m.buffer, buff, m.size);
    write(socket_fd, &m, sizeof(m));

    close(fd);
  }
  printf("read for path %s returned %ld\n", path, m.res);
}

void handle_stat(int socket_fd, MSG m) {
  int res;
  char path[PSIZE];
  struct stat stbuf;

  handle_path(m.path, path);

  res = stat(path, &stbuf);
  if (res < 0)
    perror("stat");

  // send stat struct
  m.res = res;
  memcpy(&m.st, &stbuf, sizeof(struct stat));
  write(socket_fd, &m, sizeof(m));

  printf("stat for path %s returned %ld\n", path, m.res);
}

void handle_open(int socket_fd, MSG m) {
  int fd;
  char path[PSIZE];
  handle_path(m.path, path);

  fd = open(path, m.flags, m.mode);
  m.res = fd;
  write(socket_fd, &m, sizeof(m));
}

void handle_unlink(int socket_fd, MSG m) {
  char path[PSIZE];
  ssize_t res;

  handle_path(m.path, path);

  res = unlink(path);
  if (res < 0)
    perror("unlink");

  m.res = res;

  res = write(socket_fd, &m, sizeof(m));
  ;
  if (res < 0)
    perror("write");
  printf("unlink for path %s returned %ld\n", path, m.res);
}

void handle_close(int socket_fd, MSG m) {
  (void)close(m.fd); // Ignore return value
  write(socket_fd, &m, sizeof(m));
}

int main(int argc, char const *argv[]) {

  (void)signal(SIGINT, intHandler);
  opened_files = g_hash_table_new(
      g_direct_hash, g_str_equal); // create hash table to store files

  // structure for dealing with internet addresses
  struct sockaddr_in address;

  // Creating socket file descriptor
  // https://man7.org/linux/man-pages/man2/socket.2.html
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) <
      0)
    perror("setsockopt(SO_REUSEADDR) failed");

  // Initialize struct and parameters
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  // Attaches address to socket
  // https://man7.org/linux/man-pages/man2/bind.2.html
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  // https://man7.org/linux/man-pages/man2/listen.2.html
  if (listen(server_fd, LISTEN_BACKLOG) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  while (1) {

    int new_socket;

    socklen_t addrlen = sizeof(address);

    // https://man7.org/linux/man-pages/man2/accept.2.html
    new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (new_socket < 0) {
      perror("accept");
      exit(EXIT_FAILURE);
    }

    // single threaded server...
    ssize_t res;
    MSG m;
    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    while ((res = read(new_socket, &m, sizeof(m))) > 0) {

#ifdef DEBUG
      printf("\n\n\tm.op: %d,\n\tm.path: %s,\n\tm.buffer: %s\n\tm.offset: "
             "%ld\n\tm.size: %ld\n\tm.res: %ld\n",
             m.op, m.path, m.buffer, m.offset, m.size, m.res);
      printf("\tres: %ld\n", res);
#endif

      switch (m.op) {
      case WRITE:
        printf("[ Server ]: WRITE message received from remote path: %s\n",
               m.path);
        handle_write(new_socket, m);
        break;

      case READ:
        printf("[ Server ]: READ message received from remote path: %s\n",
               m.path);
        handle_read(new_socket, m);
        break;

      case STAT:
        printf("[ Server ]: STAT message received from remote path: %s\n",
               m.path);
        handle_stat(new_socket, m);
        break;

      case OPEN:
        printf("[ Server ]: OPEN message received from remote path: %s\n",
               m.path);
        handle_open(new_socket, m);
        break;

      case UNLINK:
        printf("[ Server ]: UNLINK message received from remote path: %s\n",
               m.path);
        handle_unlink(new_socket, m);
        break;

      case CLOSE:
        printf("[ Server ]: CLOSE message received from remote path: %s\n",
               m.path);
        handle_close(new_socket, m);
        break;
      default:
        printf("[ Server ]: Operation not supported\n");
        break;
      }
    }

    close(new_socket);
  }

  // closing the listening socket
  close(server_fd);
  return 0;
}

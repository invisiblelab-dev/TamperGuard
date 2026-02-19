#include "remote.h"
#include <unistd.h>

/**
 * @brief Dummy server connection
 *
 * @return int -> socket file descriptor
 */
int connect_server() {

  int client_fd;
  struct sockaddr_in serv_addr;

  // Creating socket file descriptor
  // https://man7.org/linux/man-pages/man2/socket.2.html
  client_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (client_fd < 0) {
    printf("\n Socket creation error \n");
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  // Convert IPv4 and IPv6 addresses from text to binary form
  if (inet_pton(AF_INET, IP, &serv_addr.sin_addr) <= 0) {
    printf("\nInvalid address/ Address not supported \n");
    return -1;
  }

  // connect to server
  if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <
      0) {
    printf("\nConnection Failed \n");
    return -1;
  }

  return client_fd;
}

/**
 * @brief Close dummy sserver connection
 *
 * @param client_fd
 */
void close_server(int client_fd) {

  // closing the connected socket
  close(client_fd);
}

/**
 * @brief Remote pwrite, sends the operation to be performed in a dummy server
 *
 * @param fd       -> file descriptor
 * @param buffer   -> buffer to write
 * @param size     -> number of bytes to write
 * @param offset   -> offset value
 * @param l        -> remote layer context
 * @return ssize_t -> number of written bytes in the dummy server
 */
ssize_t remote_pwrite(int fd, const void *buffer, size_t size, off_t offset,
                      LayerContext l) {

  /**
   * Both internal state and application context might need to be changed
   */
  char *path = l.app_context;
  int *client_fd = l.internal_state;

  MSG m;
  m.op = WRITE;
  strcpy(m.path, path);
  memcpy(m.buffer, buffer, size);
  m.offset = offset;
  m.size = size;

  write(*client_fd, &m, sizeof(m));
  printf("[ Client ]: WRITE Message sent path: %s\n", m.path);

  read(*client_fd, &m, sizeof(m));
  printf("[ Client ]: Received Msg from WRITE: %s\n", m.buffer);

  return m.res;
}

/**
 * @brief Remote pread, sends the operation to be performed in a dummy server
 *
 * @param fd       -> file descriptor
 * @param buffer   -> buffer to read to
 * @param size     -> number of bytes to read
 * @param offset   -> offset values
 * @param l        -> context of the remote layer
 * @return ssize_t -> number of read bytes in the dummy server
 */
ssize_t remote_pread(int fd, void *buffer, size_t size, off_t offset,
                     LayerContext l) {
  ssize_t res;

  /**
   * Both internal state and application context might need to be changed
   */

  char *path = l.app_context;
  int *client_fd = l.internal_state;

  // Initialize MSG
  MSG m;
  m.op = READ;
  m.offset = offset;
  m.size = size;
  strncpy(m.path, path, PSIZE);

  // Send MSG
  res = write(*client_fd, &m, sizeof(m));
  if (res < 0) {
    perror("write"); // check error if write returns -1
    return res;
  }

  // Receive read result
  res = read(*client_fd, &m, sizeof(m));
  if (res < 0) {
    perror("read"); // check error if read returns -1
    return res;
  }

  // copy read result to buffer
  memcpy(buffer, m.buffer, m.size);

  // return number of read bytes
  return m.res;
}

int remote_open(const char *pathname, int flags, mode_t mode, LayerContext l) {
  int res;

  /**
   * Both internal state and application context might need to be changed
   */

  int *client_fd = l.internal_state;

  // Initialize MSG
  MSG m;
  m.op = OPEN;
  strcpy(m.path, pathname);
  m.mode = mode;
  m.flags = flags;

  // Send MSG
  res = (int)write(*client_fd, &m, sizeof(m));
  if (res < 0) {
    perror("write"); // check error if write returns -1
    return res;
  }

  // Receive read result
  res = (int)read(*client_fd, &m, sizeof(m));
  if (res < 0) {
    perror("read"); // check error if read returns -1
    return res;
  }

  // return number of read bytes
  return (int)m.res;
}

int remote_close(int fd, LayerContext l) {
  int res;

  /**
   * Both internal state and application context might need to be changed
   */

  int *client_fd = l.internal_state;

  // Initialize MSG
  MSG m;
  m.op = CLOSE;
  m.fd = fd;

  // Send MSG
  res = (int)write(*client_fd, &m, sizeof(m));
  if (res < 0) {
    perror("write"); // check error if write returns -1
    return res;
  }

  // Receive read result
  res = (int)read(*client_fd, &m, sizeof(m));
  if (res < 0) {
    perror("read"); // check error if read returns -1
    return res;
  }

  // return number of read bytes
  return (int)m.res;
}

/**
 * Terminal Layer
 * - does not receive another layer
 */

/**
 * @brief Layer initialization. Since this is a terminal layers it does not have
 * next layers.
 *
 * @return LayerContext
 */
LayerContext remote_init() {

  // memory allocation for the operations struct
  LayerOps *ops = malloc(sizeof(LayerOps));
  ops->lpread = remote_pread;
  ops->lpwrite = remote_pwrite;
  ops->lopen = remote_open;   // TODO
  ops->lclose = remote_close; // TODO

  // declaration of a new layer context
  LayerContext new_layer;
  new_layer.ops = ops;          // layer exported operations
  new_layer.next_layers = NULL; // there are no next layers

  int *socket_fd = malloc(sizeof(int)); // socket file descriptor
  *socket_fd = connect_server();        // connect to server
  new_layer.internal_state = socket_fd; // socket file descriptor is kept in the
                                        // internal state of the layer.

  return new_layer;
}

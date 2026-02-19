#include "invisible.h"
#include <fcntl.h>

// This example demonstrates how to use the invisible storage bindings.
//
// The example is written in C and uses the invisible storage bindings for C
// to create a new storage service and write and read a file using both S3 via
// OpenDAL and Solana via SDK.
//
// A very similar example can be found in the invisible-storage-bindings
// repository in the examples/c directory.
int main() {
  char filename[] = "test.txt";
  char read_data_s3[14];
  char read_data_solana[14];
  size_t data_len = 14;

  LayerContext s3_opendal_layer = s3_opendal_init();
  if (s3_opendal_layer.ops == NULL) {
    (void)fprintf(stderr, "Failed to create S3 storage service\n");
    return -1;
  }
  LayerContext solana_layer = solana_init();
  if (solana_layer.ops == NULL) {
    (void)fprintf(stderr, "Failed to create Solana storage service\n");
    return -1;
  }

  LayerOps *s3_opendal_operations = (LayerOps *)(s3_opendal_layer.ops);
  LayerOps *solana_operations = (LayerOps *)(solana_layer.ops);

  int s3_fd = s3_opendal_operations->lopen(filename, O_CREAT | O_WRONLY, 0644,
                                           s3_opendal_layer);
  int solana_fd = solana_operations->lopen(filename, O_CREAT | O_WRONLY, 0644,
                                           solana_layer);

  char write_data[] = "Hello, World!";
  size_t write_len = strlen(write_data) + 1;
  ssize_t bytes_writtenS3 = s3_opendal_operations->lpwrite(
      s3_fd, write_data, write_len, 0, s3_opendal_layer);
  ssize_t bytes_writtenSolana = solana_operations->lpwrite(
      solana_fd, write_data, write_len, 0, solana_layer);
  printf("Bytes written S3: %ld\n", bytes_writtenS3);
  printf("Bytes written Solana: %ld\n", bytes_writtenSolana);

  ssize_t bytes_readS3 = s3_opendal_operations->lpread(
      s3_fd, read_data_s3, data_len, 0, s3_opendal_layer);
  ssize_t bytes_readSolana = solana_operations->lpread(
      solana_fd, read_data_solana, data_len, 0, solana_layer);
  printf("Read data S3: %s. Bytes read: %ld\n", read_data_s3,
         bytes_readS3); // "Hello, World!"
  printf("Read data Solana: %s. Bytes read: %ld\n", read_data_solana,
         bytes_readSolana); // "Hello, World!"

  s3_opendal_operations->lclose(s3_fd, s3_opendal_layer);
  solana_operations->lclose(solana_fd, solana_layer);

  return 0;
}

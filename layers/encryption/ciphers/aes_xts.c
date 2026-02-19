#include "aes_xts.h"
#include <assert.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/types.h>
#include <stdio.h>
#include <string.h>

// XTS does not support streaming (EncryptUpdate once per EncryptInit)
// XTS does not change the block size
static int
aes_xts_crypt(const unsigned char *key, // 64 bytes (2 * 32 byte keys)
              const unsigned char *iv,  // 16 bytes (AES block size)
              const unsigned char *in, int in_len, unsigned char *out,
              int encrypt // 1 for encrypt, 0 for decrypt
) {
  if (in_len < 16) {
    ERROR_MSG("[ENCRYPTION_LAYER] Every block must have at least 16 bytes. Got "
              "%ld bytes.\n",
              in_len);
    return -1;
  }

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    ERR_print_errors_fp(stderr);
    return -1;
  }

  int (*init_fn)(EVP_CIPHER_CTX *, const EVP_CIPHER *, ENGINE *,
                 const unsigned char *, const unsigned char *);
  int (*update_fn)(EVP_CIPHER_CTX *, unsigned char *, int *,
                   const unsigned char *, int);
  int (*final_fn)(EVP_CIPHER_CTX *, unsigned char *, int *);

  if (encrypt) {
    init_fn = EVP_EncryptInit_ex;
    update_fn = EVP_EncryptUpdate;
    final_fn = EVP_EncryptFinal_ex;
  } else {
    init_fn = EVP_DecryptInit_ex;
    update_fn = EVP_DecryptUpdate;
    final_fn = EVP_DecryptFinal_ex;
  }

  if (1 != init_fn(ctx, EVP_aes_256_xts(), NULL, key, iv)) {
    ERR_print_errors_fp(stderr);
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  int len = 0;
  if (1 != update_fn(ctx, out, &len, in, in_len)) {
    ERR_print_errors_fp(stderr);
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  int out_len = len;
  if (1 != final_fn(ctx, out + len, &len)) {
    ERR_print_errors_fp(stderr);
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  out_len += len;

  EVP_CIPHER_CTX_free(ctx);
  return out_len;
}

int aes_xts_encrypt(const unsigned char *key, const unsigned char *iv,
                    const unsigned char *data, int data_len,
                    unsigned char *encrypted_data) {
  return aes_xts_crypt(key, iv, data, data_len, encrypted_data, 1);
}

int aes_xts_decrypt(const unsigned char *key, const unsigned char *iv,
                    const unsigned char *encrypted_data, int encrypted_data_len,
                    unsigned char *data) {
  return aes_xts_crypt(key, iv, encrypted_data, encrypted_data_len, data, 0);
}

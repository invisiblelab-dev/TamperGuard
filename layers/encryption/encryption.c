#include "encryption.h"
#include "../../logdef.h"
#include "ciphers/aes_xts.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

// Structure to hold the HTTP response
struct MemoryStruct {
  char *memory;
  size_t size;
};

// Callback function for curl to write response data
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb,
                                    void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if (!ptr) {
    ERROR_MSG("[ENCRYPTION] Not enough memory (realloc returned NULL)");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

// Extract the encryption key from the JSON response
// Expected JSON structure: {"data":{"data":{"key":"..."},...},...}
static char *extract_key_from_json(const char *json) {
  // Find the nested "key" field in the JSON response
  // Looking for: "data":{"data":{"key":"VALUE"}}
  const char *key_prefix = "\"key\":\"";
  char *key_start = strstr(json, key_prefix);

  if (!key_start) {
    ERROR_MSG("[ENCRYPTION] Could not find 'key' field in JSON response");
    return NULL;
  }

  key_start += strlen(key_prefix);
  char *key_end = strchr(key_start, '"');

  if (!key_end) {
    ERROR_MSG("[ENCRYPTION] Malformed JSON: could not find end of key value");
    return NULL;
  }

  size_t key_len = key_end - key_start;
  char *key = malloc(key_len + 1);
  if (!key) {
    ERROR_MSG("[ENCRYPTION] Memory allocation failed for encryption key");
    return NULL;
  }

  strncpy(key, key_start, key_len);
  key[key_len] = '\0';

  return key;
}

// Fetch encryption key from Vault server
static char *fetch_key_from_vault(const char *vault_addr, const char *api_key,
                                  const char *secret_path) {
  CURL *curl;
  CURLcode res;
  struct MemoryStruct chunk;
  char *encryption_key = NULL;

  chunk.memory = malloc(1);
  chunk.size = 0;

  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();

  if (!curl) {
    ERROR_MSG("[ENCRYPTION] Failed to initialize curl");
    free(chunk.memory);
    curl_global_cleanup();
    return NULL;
  }

  // Build the full URL
  const char *path = secret_path;

  // Remove leading slash if present
  if (path[0] == '/') {
    path++;
  }

  char url[512];
  (void)snprintf(url, sizeof(url), "%s/%s", vault_addr, path);

  // Set up the request
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

  // Set the X-Vault-Token header
  struct curl_slist *headers = NULL;
  char auth_header[512];
  (void)snprintf(auth_header, sizeof(auth_header), "X-Vault-Token: %s",
                 api_key);
  headers = curl_slist_append(headers, auth_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  // Perform the request
  DEBUG_MSG("[ENCRYPTION] Fetching encryption key from Vault: %s", url);
  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    ERROR_MSG("[ENCRYPTION] curl_easy_perform() failed: %s",
              curl_easy_strerror(res));
  } else {
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code == 200) {
      DEBUG_MSG(
          "[ENCRYPTION] Successfully retrieved encryption key from Vault");
      encryption_key = extract_key_from_json(chunk.memory);
    } else {
      ERROR_MSG("[ENCRYPTION] HTTP request failed with code: %ld", http_code);
      ERROR_MSG("[ENCRYPTION] Response: %s", chunk.memory);
    }
  }

  // Cleanup
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  free(chunk.memory);
  curl_global_cleanup();

  return encryption_key;
}

LayerContext encryption_init(LayerContext *next_layer,
                             const EncryptionConfig *config) {
  LayerContext layer_state;
  layer_state.app_context = NULL;

  EncryptionState *state = malloc(sizeof(EncryptionState));
  if (!state) {
    ERROR_MSG("[ENCRYPTION] Failed to allocate memory for encryption state");
    exit(1);
  }

  state->block_size = config->block_size;

  // Determine the encryption key source
  char *key_to_use = NULL;
  int should_free_key = 0;

  if (config->api_key) {
    // Fetch the key from Vault
    DEBUG_MSG("[ENCRYPTION] Fetching encryption key from Vault using API key");
    key_to_use = fetch_key_from_vault(config->vault_addr, config->api_key,
                                      config->secret_path);
    if (!key_to_use) {
      ERROR_MSG("[ENCRYPTION] Failed to fetch encryption key from Vault. "
                "Initialization failed.");
      free(state);
      exit(1);
    }
    should_free_key = 1;
  } else if (config->encryption_key) {
    // Use the provided key directly
    key_to_use = config->encryption_key;
  } else {
    ERROR_MSG("[ENCRYPTION] No encryption key or API key provided");
    free(state);
    exit(1);
  }

  state->key = (const unsigned char *)strdup(key_to_use);

  // Free the fetched key if we allocated it
  if (should_free_key) {
    free(key_to_use);
  }

  if (!state->key) {
    ERROR_MSG("[ENCRYPTION] Failed to allocate memory for encryption key");
    free(state);
    exit(1);
  }

  layer_state.internal_state = state;

  // LayerOps definition
  LayerOps *encryption_ops = malloc(sizeof(LayerOps));
  encryption_ops->lpread = encryption_pread;
  encryption_ops->lpwrite = encryption_pwrite;
  encryption_ops->lopen = encryption_open;
  encryption_ops->lclose = encryption_close;
  encryption_ops->lfsync = encryption_fsync;
  encryption_ops->lftruncate = encryption_ftruncate;
  encryption_ops->lfstat = encryption_fstat;
  encryption_ops->llstat = encryption_lstat;
  encryption_ops->lunlink = encryption_unlink;
  encryption_ops->lreaddir = encryption_readdir;
  encryption_ops->lrename = encryption_rename;
  encryption_ops->lchmod = encryption_chmod;

  layer_state.ops = encryption_ops;

  LayerContext *aux = malloc(sizeof(LayerContext));
  memcpy(aux, next_layer, sizeof(LayerContext));
  layer_state.next_layers = aux;
  layer_state.nlayers = 1;

  return layer_state;
}

ssize_t encryption_pread(int fd, void *buffer, size_t nbyte, off_t offset,
                         LayerContext l) {
  ssize_t res;
  EncryptionState *state = (EncryptionState *)l.internal_state;

  void *encrypted_buffer = malloc(nbyte);
  if (!encrypted_buffer) {
    return -1;
  }

  // issue a read to the next layer to get the encrypted content -> ciphertext
  // has the same size as the original text
  res = l.next_layers->ops->lpread(fd, encrypted_buffer, nbyte, offset,
                                   *l.next_layers);

  if (res <= 0) {
    free(encrypted_buffer);
    return res;
  }

  // compute the number of blocks to decrypt based on actual bytes read
  size_t whole_blocks = res / state->block_size;
  size_t last_block = res % state->block_size; // int to match cipher types

  // block decryption
  unsigned char *encrypted_ptr = (unsigned char *)encrypted_buffer;
  unsigned char *decrypted_ptr = (unsigned char *)buffer;
  uint64_t block_counter = 0;

  for (size_t i = 0; i < whole_blocks; i++) {
    unsigned char iv[16] = {0};
    memcpy(iv, &block_counter, sizeof(uint64_t));
    block_counter++;

    int out_len = aes_xts_decrypt(state->key, iv, encrypted_ptr,
                                  state->block_size, decrypted_ptr);
    if (out_len < 0) {
      free(encrypted_buffer);
      return -1;
    }
    decrypted_ptr += state->block_size;
    encrypted_ptr += state->block_size;
  }

  // decrypt last partial block if any
  if (last_block > 0) {
    unsigned char iv[16] = {0};
    memcpy(iv, &block_counter, sizeof(uint64_t));

    int out_len = aes_xts_decrypt(state->key, iv, encrypted_ptr,
                                  (int)last_block, decrypted_ptr);
    if (out_len < 0) {
      free(encrypted_buffer);
      return -1;
    }
  }

  free(encrypted_buffer);
  return res;
}

ssize_t encryption_pwrite(int fd, const void *buffer, size_t nbyte,
                          off_t offset, LayerContext l) {
  ssize_t res;
  EncryptionState *state = (EncryptionState *)l.internal_state;

  // allocate space for encrypted data
  void *encrypted_buffer = malloc(nbyte);
  if (!encrypted_buffer) {
    return -1;
  }

  size_t whole_blocks = nbyte / state->block_size;
  size_t last_block = nbyte % state->block_size;

  // block encryption
  unsigned char *decrypted_ptr = (unsigned char *)buffer;
  unsigned char *encrypted_ptr = (unsigned char *)encrypted_buffer;
  uint64_t block_counter = 0;

  for (size_t i = 0; i < whole_blocks; i++) {
    unsigned char iv[16] = {0};
    memcpy(iv, &block_counter, sizeof(uint64_t));
    block_counter++;

    int out_len = aes_xts_encrypt(state->key, iv, decrypted_ptr,
                                  state->block_size, encrypted_ptr);
    if (out_len < 0) {
      free(encrypted_buffer);
      return -1;
    }
    decrypted_ptr += state->block_size;
    encrypted_ptr += state->block_size;
  }

  // encrypt last partial block if any
  if (last_block > 0) {
    unsigned char iv[16] = {0};
    memcpy(iv, &block_counter, sizeof(uint64_t));

    int out_len = aes_xts_encrypt(state->key, iv, decrypted_ptr,
                                  (int)last_block, encrypted_ptr);
    if (out_len < 0) {
      free(encrypted_buffer);
      return -1;
    }
  }

  res = l.next_layers->ops->lpwrite(fd, encrypted_buffer, nbyte, offset,
                                    *l.next_layers);

  free(encrypted_buffer);
  return res;
}

int encryption_open(const char *pathname, int flags, mode_t mode,
                    LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lopen(pathname, flags, mode, *l.next_layers);
}

int encryption_close(int fd, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lclose(fd, *l.next_layers);
}

void encryption_destroy(LayerContext l) {
  EncryptionState *state = (EncryptionState *)l.internal_state;
  if (state) {
    free((void *)state->key);
    free(state);
  }

  if (l.ops) {
    free(l.ops);
  }

  if (l.next_layers) {
    free(l.next_layers);
  }
}

int encryption_ftruncate(int fd, off_t length, LayerContext l) {
  // TODO
  return 0;
}

int encryption_truncate(const char *path, off_t length, LayerContext l) {
  // TODO
  return 0;
}

int encryption_fstat(int fd, struct stat *stbuf, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lfstat(fd, stbuf, *l.next_layers);
}

int encryption_lstat(const char *pathname, struct stat *stbuf, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->llstat(pathname, stbuf, *l.next_layers);
}

int encryption_unlink(const char *pathname, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lunlink(pathname, *l.next_layers);
}

int encryption_readdir(const char *path, void *buf,
                       int (*filler)(void *buf, const char *name,
                                     const struct stat *stbuf, off_t off,
                                     unsigned int flags),
                       off_t offset, struct fuse_file_info *fi,
                       unsigned int flags, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lreaddir(path, buf, filler, offset, fi, flags,
                                      *l.next_layers);
}

int encryption_rename(const char *from, const char *to, unsigned int flags,
                      LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lrename(from, to, flags, *l.next_layers);
}

int encryption_chmod(const char *path, mode_t mode, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lchmod(path, mode, *l.next_layers);
}

int encryption_fsync(int fd, int isdatasync, LayerContext l) {
  l.next_layers->app_context = l.app_context;
  return l.next_layers->ops->lfsync(fd, isdatasync, *l.next_layers);
}

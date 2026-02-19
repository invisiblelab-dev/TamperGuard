#ifndef __ANTI_TAMPERING_UTILS_H__
#define __ANTI_TAMPERING_UTILS_H__

#include "../../shared/types/layer_context.h"
#include "../../shared/utils/hasher/hasher.h"
#include "anti_tampering.h"
#include <stddef.h>
#include <sys/types.h>

// FD validation
int is_valid_anti_tampering_fd(int fd);

// File mapping utilities
void free_file_mapping(FileMapping *mapping);
char *construct_hash_pathname(const AntiTamperingState *state,
                              const char *file_path_hex_hash);

// Hash file utilities
int ensure_hash_file_exists(AntiTamperingState *state, const char *path,
                            LayerContext l);

// Block hashing utilities (for block mode)
char *hash_blocks_to_hex(const void *buffer, size_t buffer_size,
                         size_t block_size, const Hasher *hasher,
                         size_t *out_hex_chars, size_t *out_concat_len);

#endif // __ANTI_TAMPERING_UTILS_H__

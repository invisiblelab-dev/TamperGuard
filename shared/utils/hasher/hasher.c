#include "hasher.h"
#include "sha256_hasher.h"
#include "sha512_hasher.h"

/**
 * @brief Initialize a hasher with the specified algorithm
 *
 * @param hasher Pointer to the hasher to initialize
 * @param algorithm Hash algorithm to use
 * @return 0 on success, -1 on error
 */
int hasher_init(Hasher *hasher, hash_algorithm_t algorithm) {
  if (!hasher) {
    return -1;
  }

  switch (algorithm) {
  case HASH_SHA256:
    return sha256_hasher_init(hasher);

  case HASH_SHA512:
    return sha512_hasher_init(hasher);

  default:
    return -1; // Invalid algorithm
  }
}

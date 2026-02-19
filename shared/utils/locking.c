#include "locking.h"
#include <stdlib.h>
#include <string.h>

/*
 * ============================================================================
 * LOCKING UTILITIES IMPLEMENTATION
 * ============================================================================
 *
 * This implementation provides a thread-safe hash table of reader-writer locks
 * indexed by file paths. Each file path gets its own pthread_rwlock_t for
 * efficient reader-writer synchronization.
 *
 * ARCHITECTURE:
 * - Hash table with chaining for collision resolution
 * - Reference counting for automatic cleanup of unused lock entries
 * - Table-level mutex for hash table structure protection
 * - Individual rwlocks for each file path
 *
 * THREAD SAFETY:
 * - All operations are thread-safe
 * - Lock acquisition/release is atomic
 * - Hash table modifications are protected by table_mutex
 * - Individual file operations use their own rwlocks
 * ============================================================================
 */

/**
 * @brief Simple hash function for file paths
 *
 * Uses djb2 algorithm for good distribution of hash values.
 *
 * @param str -> string to hash
 * @return size_t -> hash value
 */
static size_t hash_string(const char *str) {
  size_t hash = 5381;
  int c;

  while ((c = (unsigned char)*str++)) {
    hash = ((hash << 5) + hash) + c; // hash * 33 + c
  }

  return hash % LOCK_TABLE_SIZE;
}

/**
 * @brief Find an existing lock entry for the given file path
 *
 * This function assumes the table_mutex is already held by the caller.
 *
 * @param lock_table -> lock table to search
 * @param file_path -> file path to find
 * @return LockEntry* -> pointer to entry if found, NULL otherwise
 */
static LockEntry *find_lock_entry(LockTable *lock_table,
                                  const char *file_path) {
  size_t hash = hash_string(file_path);
  LockEntry *entry = lock_table->table[hash];

  while (entry) {
    if (strcmp(entry->file_path, file_path) == 0) {
      return entry;
    }
    entry = entry->next;
  }

  return NULL;
}

/**
 * @brief Create a new lock entry for the given file path
 *
 * This function assumes the table_mutex is already held by the caller.
 *
 * @param lock_table -> lock table to add to
 * @param file_path -> file path for the new entry
 * @return LockEntry* -> pointer to new entry if successful, NULL on failure
 */
static LockEntry *create_lock_entry(LockTable *lock_table,
                                    const char *file_path) {
  size_t hash = hash_string(file_path);

  // Allocate new entry
  LockEntry *entry = malloc(sizeof(LockEntry));
  if (!entry) {
    return NULL;
  }

  // Allocate and copy file path
  entry->file_path = strdup(file_path);
  if (!entry->file_path) {
    free(entry);
    return NULL;
  }

  // Initialize the reader-writer lock
  if (pthread_rwlock_init(&entry->rwlock, NULL) != 0) {
    free(entry->file_path);
    free(entry);
    return NULL;
  }

  // Initialize other fields
  entry->ref_count = 0;
  entry->next = NULL;

  // Add to hash table (at head of chain)
  entry->next = lock_table->table[hash];
  lock_table->table[hash] = entry;

  return entry;
}

/**
 * @brief Remove a lock entry from the hash table
 *
 * This function assumes the table_mutex is already held by the caller
 * and that the entry's ref_count is 0.
 *
 * @param lock_table -> lock table to remove from
 * @param file_path -> file path of entry to remove
 */
static void remove_lock_entry(LockTable *lock_table, const char *file_path) {
  size_t hash = hash_string(file_path);
  LockEntry *entry = lock_table->table[hash];
  LockEntry *prev = NULL;

  while (entry) {
    if (strcmp(entry->file_path, file_path) == 0) {
      // Remove from chain
      if (prev) {
        prev->next = entry->next;
      } else {
        lock_table->table[hash] = entry->next;
      }

      // Cleanup entry
      pthread_rwlock_destroy(&entry->rwlock);
      free(entry->file_path);
      free(entry);
      return;
    }
    prev = entry;
    entry = entry->next;
  }
}

/**
 * @brief Initialize a new locking system
 *
 * @return LockTable* -> pointer to initialized lock table, or NULL on failure
 */
LockTable *locking_init(void) {
  LockTable *lock_table = malloc(sizeof(LockTable));
  if (!lock_table) {
    return NULL;
  }

  // Initialize hash table to all NULL
  for (int i = 0; i < LOCK_TABLE_SIZE; i++) {
    lock_table->table[i] = NULL;
  }

  // Initialize table mutex
  if (pthread_mutex_init(&lock_table->table_mutex, NULL) != 0) {
    free(lock_table);
    return NULL;
  }

  return lock_table;
}

/**
 * @brief Destroy the locking system and free all resources
 *
 * @param lock_table -> lock table to destroy
 */
void locking_destroy(LockTable *lock_table) {
  if (!lock_table) {
    return;
  }

  // Lock table mutex to prevent concurrent access during cleanup
  pthread_mutex_lock(&lock_table->table_mutex);

  // Free all entries in the hash table
  for (int i = 0; i < LOCK_TABLE_SIZE; i++) {
    LockEntry *entry = lock_table->table[i];
    while (entry) {
      LockEntry *next = entry->next;

      // Destroy the rwlock and free memory
      pthread_rwlock_destroy(&entry->rwlock);
      free(entry->file_path);
      free(entry);

      entry = next;
    }
  }

  pthread_mutex_unlock(&lock_table->table_mutex);

  // Destroy table mutex
  pthread_mutex_destroy(&lock_table->table_mutex);

  // Free the table itself
  free(lock_table);
}

/**
 * @brief Acquire a read lock for the specified file path
 *
 * Multiple threads can hold read locks simultaneously for the same path.
 * Read locks will block if a write lock is currently held.
 *
 * @param lock_table -> lock table to use
 * @param file_path -> file path to lock
 * @return int -> 0 on success, -1 on failure
 */
int locking_acquire_read(LockTable *lock_table, const char *file_path) {
  if (!lock_table || !file_path) {
    return -1;
  }

  pthread_mutex_lock(&lock_table->table_mutex);

  // Find or create lock entry
  LockEntry *entry = find_lock_entry(lock_table, file_path);
  if (!entry) {
    entry = create_lock_entry(lock_table, file_path);
    if (!entry) {
      pthread_mutex_unlock(&lock_table->table_mutex);
      return -1;
    }
  }

  // Increment reference count
  entry->ref_count++;

  pthread_mutex_unlock(&lock_table->table_mutex);

  // Acquire read lock
  if (pthread_rwlock_rdlock(&entry->rwlock) != 0) {
    // If lock acquisition fails, decrement ref count
    pthread_mutex_lock(&lock_table->table_mutex);
    entry->ref_count--;
    if (entry->ref_count == 0) {
      remove_lock_entry(lock_table, file_path);
    }
    pthread_mutex_unlock(&lock_table->table_mutex);
    return -1;
  }

  return 0;
}

/**
 * @brief Acquire a write lock for the specified file path
 *
 * Write locks are exclusive - only one thread can hold a write lock
 * for a path, and it will block all other read and write attempts.
 *
 * @param lock_table -> lock table to use
 * @param file_path -> file path to lock
 * @return int -> 0 on success, -1 on failure
 */
int locking_acquire_write(LockTable *lock_table, const char *file_path) {
  if (!lock_table || !file_path) {
    return -1;
  }

  pthread_mutex_lock(&lock_table->table_mutex);

  // Find or create lock entry
  LockEntry *entry = find_lock_entry(lock_table, file_path);
  if (!entry) {
    entry = create_lock_entry(lock_table, file_path);
    if (!entry) {
      pthread_mutex_unlock(&lock_table->table_mutex);
      return -1;
    }
  }

  // Increment reference count
  entry->ref_count++;

  pthread_mutex_unlock(&lock_table->table_mutex);

  // Acquire write lock
  if (pthread_rwlock_wrlock(&entry->rwlock) != 0) {
    // If lock acquisition fails, decrement ref count
    pthread_mutex_lock(&lock_table->table_mutex);
    entry->ref_count--;
    if (entry->ref_count == 0) {
      remove_lock_entry(lock_table, file_path);
    }
    pthread_mutex_unlock(&lock_table->table_mutex);
    return -1;
  }

  return 0;
}

/**
 * @brief Release a lock for the specified file path
 *
 * This function releases both read and write locks. The caller must
 * ensure they're releasing the correct type of lock they acquired.
 *
 * @param lock_table -> lock table to use
 * @param file_path -> file path to unlock
 * @return int -> 0 on success, -1 on failure
 */
int locking_release(LockTable *lock_table, const char *file_path) {
  if (!lock_table || !file_path) {
    return -1;
  }

  pthread_mutex_lock(&lock_table->table_mutex);

  // Find the lock entry
  LockEntry *entry = find_lock_entry(lock_table, file_path);
  if (!entry) {
    pthread_mutex_unlock(&lock_table->table_mutex);
    return -1; // Entry not found
  }

  pthread_mutex_unlock(&lock_table->table_mutex);

  // Release the rwlock (works for both read and write locks)
  if (pthread_rwlock_unlock(&entry->rwlock) != 0) {
    return -1;
  }

  // Decrement reference count and potentially remove entry
  pthread_mutex_lock(&lock_table->table_mutex);
  entry->ref_count--;

  // If no more references, remove the entry to save memory
  if (entry->ref_count == 0) {
    remove_lock_entry(lock_table, file_path);
  }

  pthread_mutex_unlock(&lock_table->table_mutex);

  return 0;
}

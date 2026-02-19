#ifndef LOCKING_H
#define LOCKING_H

#include <pthread.h>
#include <stddef.h>

/*
 * ============================================================================
 * LOCKING UTILITIES - FILE PATH BASED READER-WRITER LOCKS
 * ============================================================================
 *
 * This utility provides thread-safe reader-writer locking based on file paths
 * instead of file descriptors. Each layer can maintain its own independent
 * locking system without relying on file descriptor-based locking mechanisms.
 *
 * KEY FEATURES:
 * - Reader-writer locks using pthread_rwlock_t
 * - File path-based resource identification
 * - Hash table for efficient lock lookup
 * - Thread-safe lock table management
 * - Independent locking system per layer instance
 *
 * USAGE:
 * 1. Initialize: locking_init()
 * 2. Acquire read lock: locking_acquire_read(path)
 * 3. Acquire write lock: locking_acquire_write(path)
 * 4. Release lock: locking_release(path)
 * 5. Cleanup: locking_destroy()
 *
 * LOCK COMPATIBILITY (pthread_rwlock_t):
 * ┌─────────────────┬─────────────┬─────────────────┐
 * │ Held Lock \     │ READ LOCK   │ WRITE LOCK      │
 * │ Requested Lock  │             │                 │
 * ├─────────────────┼─────────────┼─────────────────┤
 * │ READ LOCK       │   GRANTED   │    BLOCKED      │
 * ├─────────────────┼─────────────┼─────────────────┤
 * │ WRITE LOCK      │   BLOCKED   │    BLOCKED      │
 * └─────────────────┴─────────────┴─────────────────┘
 *
 * ============================================================================
 */

#define LOCK_TABLE_SIZE 16384 // Hash table size (should be power of 2)

/**
 * @brief Lock entry for a specific file path
 */
typedef struct LockEntry {
  char *file_path;         // File path (key)
  pthread_rwlock_t rwlock; // Reader-writer lock
  int ref_count;           // Reference count for cleanup
  struct LockEntry *next;  // Next entry in hash chain
} LockEntry;

/**
 * @brief Lock table structure for managing file path locks
 */
typedef struct {
  LockEntry *table[LOCK_TABLE_SIZE]; // Hash table of lock entries
  pthread_mutex_t table_mutex;       // Mutex for table modifications
} LockTable;

/**
 * @brief Initialize a new locking system
 *
 * @return LockTable* -> pointer to initialized lock table, or NULL on failure
 */
LockTable *locking_init(void);

/**
 * @brief Destroy the locking system and free all resources
 *
 * @param lock_table -> lock table to destroy
 */
void locking_destroy(LockTable *lock_table);

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
int locking_acquire_read(LockTable *lock_table, const char *file_path);

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
int locking_acquire_write(LockTable *lock_table, const char *file_path);

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
int locking_release(LockTable *lock_table, const char *file_path);

#endif // LOCKING_H

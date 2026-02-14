#ifndef DIESEL_KSYNC_H
#define DIESEL_KSYNC_H

#include "../../common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef DIESEL_INTERNAL_IMPL_EMULATED

#ifdef __cplusplus
extern "C" {
#endif

typedef void KMutex;

static inline KMutex* InitKMutex(void) { return NULL; }

static inline bool DestroyKMutex(KMutex* mutex) { (void)mutex; return true; }

static inline void LockKMutex(KMutex* mutex) { (void)mutex; }

static inline void UnlockKMutex(KMutex* mutex) { (void)mutex; }

#ifdef __cplusplus
}
#endif


#else

/**
 * @brief A mutual exclusion lock for `KThreads`.
 * 
 * @attention This is currently a `NO-OP` as Diesel is using the `Emulated` backend
 * 
 * @warning DO NOT COPY, MOVE, OR DEREFERENCE THIS OBJECT.
 * @warning USE WITH `KThreads` ONLY
*/
typedef struct KMutex KMutex;

/**
 * @brief Create a new KMutex.
 * 
 * @attention This is currently a `NO-OP` as Diesel is using the `Emulated` backend
 * 
 * Allocates and initializes a `KMutex` object appropriate for the current platform.
 *
 * @return Pointer to the newly created `KMutex`, or `NULL` if allocation failed.
 *
*/
KMutex* InitKMutex(void);

/**
 * @brief Destroys a `KMutex`.
 * 
 * @attention This is currently a `NO-OP` as Diesel is using the `Emulated` backend
 * 
 * Frees the memory and cleans up the internal resources
 * associated with the given `KMutex`.
 *
 * @param mutex Pointer to the `KMutex` to destroy.
 * @return `true` if the mutex was successfully destroyed, `false` if the
 * pointer was `NULL` or an error occurred during cleanup.
*/
bool DestroyKMutex(KMutex* mutex);

/**
 * @brief Locks a `KMutex`.
 * 
 * @attention This is currently a NO-OP as Diesel is using the Emulated backend
 * 
 * Blocks the calling thread until the mutex becomes available, then acquires it.
 *
 * @param mutex Pointer to the `KMutex` to lock.
 * @note Always call @ref UnlockKMutex after finishing the critical section
 * to avoid deadlocks.
*/
void LockKMutex(KMutex* mutex);
/**
 * @brief Unlocks a `KMutex`.
 * 
 * @attention This is currently a NO-OP as Diesel is using the Emulated backend
 * 
 * Releases a previously acquired `KMutex`, allowing other threads to acquire it.
 *
 * @param mutex Pointer to the `KMutex` to unlock.
 * 
 * @note This unlocks a mutex previously acquired by @ref LockKMutex
 */
void UnlockKMutex(KMutex* mutex);

#endif

#endif // DIESEL_KSYNC_H
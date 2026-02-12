#ifndef DIESEL_KSYNC_H
#define DIESEL_KSYNC_H

#include <stdbool.h>

#ifdef DIESEL_INTERNAL_IMPL_UNIX

#include <stdlib.h>

#if defined(SYNC_USE_USERMODE_LOCKS) && defined(__linux__)

/* ================= User-mode futex mutex (Linux) ================= */

#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <stdatomic.h>

/* Futex helpers */
static inline int _futex_wait(atomic_int* addr, int expected) {
    return syscall(SYS_futex, addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static inline int _futex_wake(atomic_int* addr, int count) {
    return syscall(SYS_futex, addr, FUTEX_WAKE, count, NULL, NULL, 0);
}

/* state: 0 = unlocked, 1 = locked */
typedef struct KMutex {
    atomic_int state;
} KMutex;

static inline KMutex* InitKMutex(void) {
    KMutex* m = (KMutex*)DIESEL_MEM_ALLOC(sizeof(KMutex));
    if (!m) return NULL;
    atomic_init(&m->state, 0);
    return m;
}

static inline bool DestroyKMutex(KMutex* mutex) {
    if (!mutex) return false;
    free(mutex);
    return true;
}

static inline void LockKMutex(KMutex* mutex) {
    int expected = 0;

    // Fast path
    if (atomic_compare_exchange_strong(&mutex->state, &expected, 1)) {
        return;
    }

    // Slow path
    for (;;) {
        while (atomic_load(&mutex->state) != 0) {
            _futex_wait(&mutex->state, 1);
        }
        expected = 0;
        if (atomic_compare_exchange_strong(&mutex->state, &expected, 1)) {
            return;
        }
    }
}
static inline void UnlockKMutex(KMutex* mutex) {
    atomic_store(&mutex->state, 0);
    _futex_wake(&mutex->state, 1);
}

#else
#include <pthread.h>

typedef struct KMutex {
    pthread_mutex_t mtx;
} KMutex;

static inline KMutex* InitKMutex(void) {
    KMutex* m = (KMutex*)DIESEL_MEM_ALLOC(sizeof(KMutex));
    if (!m) return NULL;
    if (pthread_mutex_init(&m->mtx, NULL) != 0) {
        free(m);
        return NULL;
    }
    return m;
}

static inline bool DestroyKMutex(KMutex* mutex) {
    if (!mutex) return false;
    pthread_mutex_destroy(&mutex->mtx);
    free(mutex);
    return true;
}

static inline void LockKMutex(KMutex* mutex) {
    pthread_mutex_lock(&mutex->mtx);
}

static inline void UnlockKMutex(KMutex* mutex) {
    pthread_mutex_unlock(&mutex->mtx);
}

#endif // SYNC_USE_USERMODE_LOCKS

#else

/**
 * @brief A mutual exclusion lock for `KThreads`.
 * 
 * @warning DO NOT COPY, MOVE, OR DEREFERENCE THIS OBJECT.
 * @warning USE WITH `KThreads` ONLY
*/
typedef struct KMutex KMutex;

/**
 * @brief Create a new KMutex.
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
 * Releases a previously acquired `KMutex`, allowing other threads to acquire it.
 *
 * @param mutex Pointer to the `KMutex` to unlock.
 * 
 * @note This unlocks a mutex previously acquired by @ref LockKMutex
 */
void UnlockKMutex(KMutex* mutex);

#endif // DIESEL_INTERNAL_IMPL_UNIX
#endif // DIESEL_KSYNC_H

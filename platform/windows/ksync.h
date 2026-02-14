#ifndef DIESEL_KSYNC_H
#define DIESEL_KSYNC_H

#include <stdbool.h>

#ifdef DIESEL_INTERNAL_IMPL_WINDOWS
#define WINDOWS_LEAN_AND_MEAN
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SYNC_USE_USERMODE_LOCKS

typedef struct KMutex {
    CRITICAL_SECTION cs;
} KMutex;

KMutex* InitKMutex() {
    KMutex* m = (KMutex*)DIESEL_MEM_ALLOC(sizeof(KMutex));
    InitializeCriticalSection(&m->cs);
    return m;
}

bool DestroyKMutex(KMutex* mutex) {
    if (mutex == NULL) {
        return false;
    }
    DeleteCriticalSection(&mutex->cs);
    DIESEL_MEM_FREE(mutex);
    return true;
}

void LockKMutex(KMutex* mutex) {
    EnterCriticalSection(&mutex->cs);
}

void UnlockKMutex(KMutex* mutex) {
    LeaveCriticalSection(&mutex->cs);
}

#else

typedef struct KMutex {
    HANDLE hMutex;
} KMutex;

KMutex* InitKMutex() {
    KMutex* m = (KMutex*)DIESEL_MEM_ALLOC(sizeof(KMutex));
    m->hMutex = CreateMutex(NULL, false, NULL);
    return m;
}

bool DestroyKMutex(KMutex* mutex) {
    if (mutex == NULL) {
        return false;
    }

    BOOL ok = CloseHandle(mutex->hMutex);
    DIESEL_MEM_FREE(mutex);

    return ok != 0;
}


void LockKMutex(KMutex* mutex) {
    WaitForSingleObject(mutex->hMutex, INFINITE);
}

void UnlockKMutex(KMutex* mutex) {
    ReleaseMutex(mutex->hMutex);
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

#endif // DIESEL_INTERNAL_IMPL_WINDOWS
#endif // DIESEL_KSYNC_H
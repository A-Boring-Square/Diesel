#ifndef DIESEL_H
#define DIESEL_H

/**
 * @file diesel.h
 * @brief Diesel - Header-only cross-platform concurrency library.
 *
 * This is the main include file for Diesel. It provides:
 *  - Version macros
 *  - Configurable memory allocation
 *  - Platform selection
 *  - Implementation inclusion for threads, mutexes, and job systems
 *
 * To enable the implementation, define `DIESEL_IMPLEMENTATION` in **one**
 * source file before including this header.
*/

/** @brief Version string in "MAJOR.MINOR.PATCH" format. */
#define DIESEL_VERSION_STRING "1.0.0"
/** @brief Major version number. */
#define DIESEL_VERSION_MAJOR 1
/** @brief Minor version number. */
#define DIESEL_VERSION_MINOR 0
/** @brief Patch version number. */
#define DIESEL_VERSION_PATCH 0
/**
 * @brief Numeric version for easy comparisons.
 * Encoded as MMmmpp: (major * 10000 + minor * 100 + patch)
 * Example: 1.0.0 -> 10000, 1.2.3 -> 10203
*/
#define DIESEL_VERSION_NUM (DIESEL_VERSION_MAJOR * 10000 + DIESEL_VERSION_MINOR * 100 + DIESEL_VERSION_PATCH)

/**
 * @brief Allocate memory of given size in bytes.
 * @note Can be overridden by defining MEM_ALLOC before including diesel.h.
 * @returns void*
*/
#ifndef DIESEL_MEM_ALLOC
#define DIESEL_MEM_ALLOC(size) malloc(size)
#endif

/**
 * @brief Reallocate a previously allocated memory block.
 * @note Can be overridden by defining MEM_REALLOC before including diesel.h.
 * @returns void*
*/
#ifndef DIESEL_MEM_REALLOC
#define DIESEL_MEM_REALLOC(ptr, size) realloc(ptr, size)
#endif

/**
 * @brief Free memory allocated with MEM_ALLOC or MEM_REALLOC.
 * @note Can be overridden by defining MEM_FREE before including diesel.h.
*/
#ifndef DIESEL_MEM_FREE
#define DIESEL_MEM_FREE(ptr) free(ptr)
#endif

/**
 * @def SYNC_USE_USERMODE_LOCKS
 * @brief Enable user-mode, fast mutexes on platforms that support them.
 *
 * When defined, Diesel will use OS-provided user-mode locks (e.g.,
 * CRITICAL_SECTION on Windows) instead of heavier kernel-based locks.
 *
 * This will not take effect for Unix-based OSs except Linux, which will use
 * Linux's `futex` API.
 *
 * On platforms without native user-mode locking support or in emulated
 * environments, this macro has no effect.
 *
 * @note This macro should be defined **before including** diesel.h
*/

#ifdef SYNC_USE_USERMODE_LOCKS
#define SYNC_USE_USERMODE_LOCKS
#endif

/**
 * @brief Default stack size for `KThreads`.
 * 
 * This macro defines the stack size to use when creating kernel threads.
 * It is platform-specific:
 *   - On Windows, it defaults to 1 MB.
 *   - On POSIX systems, it defaults to `PTHREAD_STACK_MIN` if available, 
 *     otherwise 1 MB.
 * @note This does not apply to KThreads when runing under the Emulated backend
 * as they just use the program's stack
 * Users can override this macro by defining `KTHREAD_STACK_SIZE` 
 * before including `diesel.h`.
 *
*/
#ifndef KTHREAD_STACK_SIZE
#if defined(_WIN32)
#define KTHREAD_STACK_SIZE (1024 * 1024)
#else
#include <limits.h>
#define KTHREAD_STACK_SIZE (PTHREAD_STACK_MIN > 0 ? PTHREAD_STACK_MIN : 1024*1024)
#endif
#endif

#ifndef DIESEL_NO_THREAD_LOCAL_STORAGE
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define TLS _Thread_local
#elif defined(_MSC_VER)
#define TLS __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define TLS __thread
#else
#if defined(_MSC_VER)
#pragma message("Warning: No thread-local storage support detected, TLS disabled")
#elif defined(__GNUC__) || defined(__clang__)
#warning "No thread-local storage support detected, TLS disabled"
#endif
#define TLS
#endif
#else
    #define TLS
#endif


/**
 * @def DIESEL_IMPLEMENTATION
 * Define this in **one source file** before including diesel.h
 * to include the implementation of Diesel.
*/
#ifdef DIESEL_IMPLEMENTATION
// Internal macro to tell platform.h we are including the implementation
#define DIESEL_INTERNAL_IMPL_PLATFORM
#include "platform/platform.h"
#else
#include "platform/platform.h"
#endif

#endif // DIESEL_H

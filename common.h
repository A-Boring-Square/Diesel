#ifndef DIESEL_COMMON_H
#define DIESEL_COMMON_H
#include <stdint.h>

/**
 * @struct FiberContext
 * @brief Execution context for a Diesel Fiber.
 *
 * fibers are user-space scheduled and managed by Diesel rather than
 * the operating system. They do not correspond 1:1 with OS threads and are
 * typically multiplexed onto a smaller number of kernel threads.
 *
 * The @c ID field is an internal Diesel-assigned identifier unique within
 * the runtime. It does NOT correspond to any OS thread ID.
 *
 * The @c user_data field is a user-defined pointer to data available to
 * the thread.
*/
typedef struct FiberContext {
    uintptr_t ID; ///< Diesel-assigned fiber identifier
    void* user_data; ///< User-provided pointer
} FiberContext;

/**
 * @struct KThreadContext
 * @brief Execution context for a Diesel kernel-scheduled thread.
 *
 * Kernel threads are scheduled preemptively by the operating system kernel.
 * Each Diesel kernel thread maps 1:1 to an OS-managed thread and executes
 * independently on available CPU cores.
 *
 * The @c ID field contains the OS-assigned thread identifier for this kernel
 * thread. On the Emulated platform, @c ID is a UUID-like value similar to
 * GThreads.
 *
 * The @c user_data field is a user-defined pointer to data available to
 * the thread.
*/
typedef struct KThreadContext {
    uintptr_t ID;     ///< OS-assigned thread ID (or emulated UUID)
    void* user_data;  ///< User-provided pointer to thread data
} KThreadContext;

/**
 * @enum ThreadPriority
 * @brief Abstract thread priority levels.
 *
 * These values express relative priority only. Each backend maps them
 * to the closest native priority supported by the operating system or
 * runtime scheduler. Exact scheduling behavior is platform-dependent.
*/
typedef enum ThreadPriority {
    THREAD_PRIORITY_LOW,     ///< Lower priority than normal execution
    THREAD_PRIORITY_DEFAULT,  ///< Default scheduling priority
    THREAD_PRIORITY_HIGH,    ///< Higher priority than normal execution
} ThreadPriority;
#endif // DIESEL_COMMON_H
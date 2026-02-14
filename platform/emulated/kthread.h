#ifndef DIESEL_KTHREAD_H
#define DIESEL_KTHREAD_H
#include "../../common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>


typedef void (*KThreadWorker)(KThreadContext* ctx);

#ifdef DIESEL_INTERNAL_IMPL_EMULATED

#ifdef __cplusplus
extern "C" {
#endif

typedef enum KThreadState {
    KTHREAD_READY,
    KTHREAD_RUNNING,
    KTHREAD_SLEEPING,
    KTHREAD_DONE
} KThreadState;

typedef struct KThread {
    KThreadContext ctx;
    KThreadWorker worker;
    KThreadState state;
    int sleep_ticks;
    int exit_code;
    bool joined;
    ThreadPriority priority;
} KThread;

typedef struct KThreadScheduler {
    KThread** threads;
    size_t count;
    size_t capacity;
    size_t current_index;
} KThreadScheduler;

static KThreadScheduler g_scheduler = {0};

static bool ensure_capacity(size_t min_capacity) {
    if (g_scheduler.capacity >= min_capacity) return true;
    size_t new_capacity = g_scheduler.capacity ? g_scheduler.capacity * 2 : 4;
    while (new_capacity < min_capacity) new_capacity *= 2;
    KThread** new_threads = (KThread**)DIESEL_MEM_REALLOC(g_scheduler.threads, new_capacity * sizeof(KThread*));
    if (!new_threads) return false;
    g_scheduler.threads = new_threads;
    g_scheduler.capacity = new_capacity;
    return true;
}

// Create a new emulated thread
KThread* CreateKThread(KThreadWorker worker, void* user_ptr) {
    if (!ensure_capacity(g_scheduler.count + 1)) return NULL;

    KThread* t = (KThread*)DIESEL_MEM_ALLOC(sizeof(KThread));
    if (!t) return NULL;

    static uintptr_t next_id = 1;
    t->ctx.ID = next_id++;
    t->ctx.user_data = user_ptr;

    t->worker = worker;
    t->state = KTHREAD_READY;
    t->sleep_ticks = 0;
    t->exit_code = 0;
    t->joined = false;
    t->priority = THREAD_PRIORITY_DEFAULT;

    g_scheduler.threads[g_scheduler.count++] = t;
    return t;
}

// Set thread priority
bool SetKThreadPriority(KThread* thread, ThreadPriority priority) {
    if (!thread) return false;
    thread->priority = priority;
    return true;
}

// Start thread
bool StartKThread(KThread* thread) {
    if (!thread || thread->state != KTHREAD_READY) return false;
    thread->state = KTHREAD_RUNNING;
    return true;
}

// Yield current thread
void YieldKThread() {
    if (g_scheduler.count == 0) return;
    KThread* t = g_scheduler.threads[g_scheduler.current_index];
    if (t && t->state == KTHREAD_RUNNING) t->state = KTHREAD_READY;
}

// Sleep current thread for ticks
void SleepKThread(int ticks) {
    if (g_scheduler.count == 0) return;
    KThread* t = g_scheduler.threads[g_scheduler.current_index];
    if (t) {
        t->sleep_ticks = ticks;
        t->state = KTHREAD_SLEEPING;
    }
}

// Mark thread done manually
bool DestroyKThread(KThread* thread) {
    if (!thread) return false;
    thread->state = KTHREAD_DONE;
    return true;
}

// Join a thread (blocks until it finishes)
int JoinKThread(KThread* thread) {
    if (!thread) return -1;
    thread->joined = true;
    while (thread->state != KTHREAD_DONE) {
        KThreadTick();
    }
    return thread->exit_code;
}

// Tick scheduler once (priority-aware round-robin)
void KThreadTick() {
    if (g_scheduler.count == 0) return;

    // Find next ready thread with highest priority
    size_t best_index = SIZE_MAX;
    ThreadPriority best_prio = THREAD_PRIORITY_LOW - 1;

    for (size_t i = 0; i < g_scheduler.count; ++i) {
        KThread* t = g_scheduler.threads[i];
        if (!t) continue;

        if (t->state == KTHREAD_SLEEPING) {
            t->sleep_ticks--;
            if (t->sleep_ticks <= 0) t->state = KTHREAD_READY;
        }

        if (t->state == KTHREAD_READY && t->priority > best_prio) {
            best_index = i;
            best_prio = t->priority;
        }
    }

    if (best_index != SIZE_MAX) {
        g_scheduler.current_index = best_index;
        KThread* t = g_scheduler.threads[best_index];
        t->state = KTHREAD_RUNNING;
        t->worker(&t->ctx);
        if (t->state == KTHREAD_RUNNING) t->state = KTHREAD_READY;
        else if (t->state != KTHREAD_SLEEPING) t->state = KTHREAD_DONE;
    }

    // Cleanup DONE threads not joined
    size_t shift = 0;
    for (size_t i = 0; i < g_scheduler.count; ++i) {
        KThread* t = g_scheduler.threads[i];
        if (t && t->state == KTHREAD_DONE && !t->joined) {
            DIESEL_MEM_FREE(t);
            shift++;
        } else if (shift > 0) {
            g_scheduler.threads[i - shift] = g_scheduler.threads[i];
        }
    }
    g_scheduler.count -= shift;
}

#ifdef __cplusplus
}
#endif

#else

typedef struct KThread KThread;

/**
 * @brief Opaque handle to a kernel thread.
*/
typedef struct KThread KThread;

/**
 * @brief Create a new kernel thread object.
 *
 * This function allocates and initializes a thread object but does not
 * necessarily start execution. Use @ref StartKThread to begin running it.
 *
 * @param worker   Function to execute in the new thread.
 * @param user_ptr User-defined pointer passed to the worker.
 *
 * @return Pointer to a new @ref KThread on success, or NULL on failure.
*/
KThread* CreateKThread(KThreadWorker worker, void* user_ptr);

/**
 * @brief Destroy a kernel thread object.
 *
 * The thread should be stopped or joined before calling this function.
 *
 * @param thread Pointer to the thread to destroy.
 *
 * @return true on success, false on failure.
*/
bool DestroyKThread(KThread* thread);

/**
 * @brief Set the priority of a kernel thread.
 *
 * The effect and supported priority levels are platform-dependent.
 *
 * @param thread   Pointer to the thread.
 * @param priority New priority value.
 *
 * @return true on success, false on failure.
*/
bool SetKThreadPriority(KThread* thread, ThreadPriority priority);

/**
 * @brief Start execution of a kernel thread.
 *
 * After this call, the thread will begin running the worker function
 * passed to @ref CreateKThread.
 *
 * @param thread Pointer to the thread to start.
 *
 * @return true on success, false on failure.
*/
bool StartKThread(KThread* thread);

/**
 * @brief Wait for a kernel thread to finish execution.
 *
 * This function blocks the calling thread until the target thread exits.
 *
 * @param thread Pointer to the thread to join.
 *
 * @return An integer result or exit code provided by the thread, or a
 *         platform-specific error code on failure.
*/
int JoinKThread(KThread* thread);

/**
 * @brief Yield execution to another thread.
 *
 * This hints to the scheduler that the current thread is willing to
 * give up the CPU.
*/
void YieldKThread();

/**
 * @brief Sleep the current thread for a given duration.
 *
 * @param delay_ms Time to sleep in milliseconds.
*/
void SleepKThread(int delay_ms);



#endif

#endif // DIESEL_KTHREAD_H

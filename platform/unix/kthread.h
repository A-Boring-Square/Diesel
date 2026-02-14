#ifndef DIESEL_KTHREAD_H
#define DIESEL_KTHREAD_H
#include "../../common.h"
#include <stdint.h>
#include <stdbool.h>

typedef void (*KThreadWorker)(KThreadContext*);

#ifdef DIESEL_INTERNAL_IMPL_UNIX

#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>

typedef struct KThread {
    pthread_t thread;
    pthread_mutex_t start_mtx;
    pthread_cond_t  start_cv;
    bool started;
    bool finished;

    KThreadWorker worker;
    KThreadContext* ctx;
} KThread;

static void* _thread_spinup_and_setup_func(void* param) {
    KThread* thread = (KThread*)param;

    // Wait until StartKThread signals
    pthread_mutex_lock(&thread->start_mtx);
    while (!thread->started) {
        pthread_cond_wait(&thread->start_cv, &thread->start_mtx);
    }
    pthread_mutex_unlock(&thread->start_mtx);

    if (thread && thread->worker) {
        thread->worker(thread->ctx);
    }

    pthread_mutex_lock(&thread->start_mtx);
    thread->finished = true;
    pthread_mutex_unlock(&thread->start_mtx);

    return NULL;
}

KThread* CreateKThread(KThreadWorker worker, void* user_ptr) {
    KThread* thread = (KThread*)DIESEL_MEM_ALLOC(sizeof(KThread));
    if (!thread) { return NULL; }

    thread->ctx = (KThreadContext*)DIESEL_MEM_ALLOC(sizeof(KThreadContext));
    if (!thread->ctx) {
        DIESEL_MEM_FREE(thread);
        return NULL;
    }

    thread->worker = worker;
    thread->ctx->user_data = user_ptr;
    thread->started = false;
    thread->finished = false;

    pthread_mutex_init(&thread->start_mtx, NULL);
    pthread_cond_init(&thread->start_cv, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);

#ifdef KTHREAD_STACK_SIZE
    if (KTHREAD_STACK_SIZE > 0) {
        pthread_attr_setstacksize(&attr, KTHREAD_STACK_SIZE);
    }
#endif

    int res = pthread_create(&thread->thread, &attr, _thread_spinup_and_setup_func, thread);
    pthread_attr_destroy(&attr);

    if (res != 0) {
        pthread_cond_destroy(&thread->start_cv);
        pthread_mutex_destroy(&thread->start_mtx);
        DIESEL_MEM_FREE(thread->ctx);
        DIESEL_MEM_FREE(thread);
        return NULL;
    }

    return thread;
}

bool DestroyKThread(KThread* thread) {
    if (!thread) { return false; }

    // Ensure thread is finished
    pthread_join(thread->thread, NULL);

    pthread_cond_destroy(&thread->start_cv);
    pthread_mutex_destroy(&thread->start_mtx);

    if (thread->ctx) {
        DIESEL_MEM_FREE(thread->ctx);
    }
    DIESEL_MEM_FREE(thread);

    return true;
}

bool SetKThreadPriority(KThread* thread, ThreadPriority priority) {
    if (!thread) { return false; }

    int policy;
    struct sched_param param;

    if (pthread_getschedparam(thread->thread, &policy, &param) != 0) {
        return false;
    }

    int min = sched_get_priority_min(policy);
    int max = sched_get_priority_max(policy);

    int prio = (min + max) / 2;

    switch (priority) {
        case THREAD_PRIORITY_LOW:
            prio = min;
            break;
        case THREAD_PRIORITY_DEFAULT:
            prio = (min + max) / 2;
            break;
        case THREAD_PRIORITY_HIGH:
            prio = max;
            break;
        default:
            prio = (min + max) / 2;
            break;
    }

    param.sched_priority = prio;
    return pthread_setschedparam(thread->thread, policy, &param) == 0;
}

bool StartKThread(KThread* thread) {
    if (!thread) { return false; }

    pthread_mutex_lock(&thread->start_mtx);
    thread->started = true;
    pthread_cond_signal(&thread->start_cv);
    pthread_mutex_unlock(&thread->start_mtx);

    return true;
}

int JoinKThread(KThread* thread) {
    if (!thread) { return -1; }

    int res = pthread_join(thread->thread, NULL);
    if (res != 0) {
        return -1;
    }

    return 0; // pthread threads don’t have a Win32-style exit code here
}

void YieldKThread() {
    sched_yield();
}

void SleepKThread(int delay_ms) {
    struct timespec ts;
    ts.tv_sec = delay_ms / 1000;
    ts.tv_nsec = (delay_ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

#else

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

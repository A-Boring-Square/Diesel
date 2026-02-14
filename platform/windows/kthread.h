#ifndef DIESEL_KTHREAD_H
#define DIESEL_KTHREAD_H
#include "../../common.h"
#include <stdint.h>
#include <stdbool.h>

typedef void (*KThreadWorker)(KThreadContext*);

#ifdef DIESEL_INTERNAL_IMPL_WINDOWS
#define WINDOWS_LEAN_AND_MEAN
#include <Windows.h>

typedef struct KThread {
    HANDLE handle;
    DWORD id;
    KThreadWorker worker;
    KThreadContext* ctx;   
} KThread;

DWORD WINAPI _thread_spinup_and_setup_func(LPVOID param) {
    KThread* thread = (KThread*)param;
    if (thread && thread->worker) {
        thread->worker(thread->ctx);  // call the user’s worker
    }
    return 0;
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

    thread->handle = CreateThread(
        NULL,
        KTHREAD_STACK_SIZE,
        _thread_spinup_and_setup_func,
        thread,
        CREATE_SUSPENDED,
        &thread->id
    );

    if (!thread->handle) {
        DIESEL_MEM_FREE(thread->ctx);
        DIESEL_MEM_FREE(thread);
        return NULL;
    }

    return thread;
}

bool DestroyKThread(KThread* thread) {
    if (!thread) { return false; }
    if (thread->handle) {
        DWORD res = WaitForSingleObject(thread->handle, INFINITE);
        if (res == WAIT_FAILED) {

        }
        CloseHandle(thread->handle);
    }
    if (thread->ctx) {
        DIESEL_MEM_FREE(thread->ctx);
    }
    DIESEL_MEM_FREE(thread);

    return true;
}

bool SetKThreadPriority(KThread* thread, ThreadPriority priority) {
    if (!thread || !thread->handle) { return false; }

    int winPriority = THREAD_PRIORITY_NORMAL;

    switch (priority) {
        case THREAD_PRIORITY_LOW:
            winPriority = THREAD_PRIORITY_BELOW_NORMAL;
            break;
        case THREAD_PRIORITY_DEFAULT:
            winPriority = THREAD_PRIORITY_NORMAL;
            break;
        case THREAD_PRIORITY_HIGH:
            winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
            break;
        default:
            winPriority = THREAD_PRIORITY_NORMAL;
            break;
    }

    return SetThreadPriority(thread->handle, winPriority) != 0;
}

bool StartKThread(KThread* thread) {
    if (!thread || !thread->handle) { return false; }

    DWORD res = ResumeThread(thread->handle);
    return (res != (DWORD)-1);
}

int JoinKThread(KThread* thread) {
    if (!thread || !thread->handle) { return -1; }

    DWORD res = WaitForSingleObject(thread->handle, INFINITE);
    if (res == WAIT_FAILED) {
        return -1;
    }

    DWORD exitCode = 0;
    if (!GetExitCodeThread(thread->handle, &exitCode)) {
        return -1;
    }

    return (int)exitCode;
}

void YieldKThread() {
    SwitchToThread();
}

void SleepKThread(int delay_ms) {
    Sleep(delay_ms);
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
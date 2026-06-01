#ifndef DIESEL_FIBER_H
#define DIESEL_FIBER_H
#include "../common.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

typedef void (*FiberWorker)(FiberContext* ctx);

#ifdef DIESEL_INTERNAL_IMPL_PLATFORM
#ifdef __cplusplus
extern "C" {
#endif

typedef enum FiberState {
    FIBER_READY,
    FIBER_RUNNING,
    FIBER_SUSPENDED,
    FIBER_FINISHED
} FiberState;

typedef struct Fiber Fiber;
struct Fiber {
    FiberContext ctx;
    FiberWorker worker;
    ThreadPriority priority;
    FiberState state;
    void* os_fiber_handle;  // Stores Windows fiber pointer OR POSIX ucontext_t pointer
    void* stack_ptr;        // Tracked explicitly for cleanup on Linux/Unix
    Fiber* next;            // Clean standard pointer link
};

typedef struct FiberSystem {
    Fiber* head;            // Clean FIFO head pointer
    Fiber* tail;            // Clean FIFO tail pointer
    KMutex* queue_mutex;    // Automatically handles No-Ops under Emulation!
    KThread** workers;
    int worker_count;
    bool running;
} FiberSystem;

static FiberSystem g_fiber_system = {0};

/* ---------------- Storage and Trackers ---------------- */
#ifdef DIESEL_INTERNAL_IMPL_EMULATED
static Fiber* g_emulated_current_fiber = NULL;
#else
#include <time.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#define _GNU_SOURCE
#include <ucontext.h>
#include <unistd.h>
#endif

#if defined(_MSC_VER)
__declspec(thread) void* t_scheduler_fiber = NULL;
__declspec(thread) Fiber* t_current_running_fiber = NULL;
#else
__thread void* t_scheduler_fiber = NULL;
__thread Fiber* t_current_running_fiber = NULL;
#endif
#endif

static void PushFiber(Fiber* f) {
    if (!f) return;
    f->next = NULL;

    LockKMutex(g_fiber_system.queue_mutex);
    if (!g_fiber_system.tail) {
        g_fiber_system.head = g_fiber_system.tail = f;
    } else {
        g_fiber_system.tail->next = f;
        g_fiber_system.tail = f;
    }
    UnlockKMutex(g_fiber_system.queue_mutex);
}

static Fiber* PopFiber(void) {
    LockKMutex(g_fiber_system.queue_mutex);
    if (!g_fiber_system.head) {
        UnlockKMutex(g_fiber_system.queue_mutex);
        return NULL;
    }

    Fiber* f = g_fiber_system.head;
    g_fiber_system.head = f->next;
    if (!g_fiber_system.head) {
        g_fiber_system.tail = NULL;
    }
    UnlockKMutex(g_fiber_system.queue_mutex);

    f->next = NULL;
    return f;
}

#ifndef DIESEL_INTERNAL_IMPL_EMULATED
#if defined(_WIN32) || defined(_WIN64)
void WINAPI NativeFiberEntry(LPVOID param) {
    Fiber* f = (Fiber*)param;
    if (f && f->worker) {
        f->worker(&f->ctx);
    }
    f->state = FIBER_FINISHED;
    if (t_scheduler_fiber) {
        SwitchToFiber(t_scheduler_fiber);
    }
}
#else 
void NativeFiberEntryPOSIX(void) {
    Fiber* f = t_current_running_fiber;
    if (f && f->worker) f->worker(&f->ctx);
    f->state = FIBER_FINISHED;
    swapcontext((ucontext_t*)f->os_fiber_handle, (ucontext_t*)t_scheduler_fiber);
}
#endif
#endif

void FiberSchedulerLoop(KThreadContext* _unused) {
    (void)_unused;

#ifndef DIESEL_INTERNAL_IMPL_EMULATED
#if defined(_WIN32) || defined(_WIN64)
    if (!IsThreadAFiber()) {
        t_scheduler_fiber = ConvertThreadToFiber(NULL);
    } else {
        t_scheduler_fiber = GetCurrentFiber();
    }
#else 
    if (!t_scheduler_fiber) {
        t_scheduler_fiber = DIESEL_MEM_ALLOC(sizeof(ucontext_t));
    }
#endif
#endif

    while (g_fiber_system.running) {
        Fiber* f = PopFiber();
        if (!f) {
            SleepKThread(1);
            continue;
        }

        if (f->state == FIBER_READY || f->state == FIBER_SUSPENDED) {
            f->state = FIBER_RUNNING;
            
#ifdef DIESEL_INTERNAL_IMPL_EMULATED
            g_emulated_current_fiber = f;
            f->worker(&f->ctx);
            if (f->state == FIBER_RUNNING) f->state = FIBER_FINISHED;
            g_emulated_current_fiber = NULL;
#else
            t_current_running_fiber = f;
#if defined(_WIN32) || defined(_WIN64)
            SwitchToFiber(f->os_fiber_handle);
#else
            swapcontext((ucontext_t*)t_scheduler_fiber, (ucontext_t*)f->os_fiber_handle);
#endif
            t_current_running_fiber = NULL;
#endif
        }

        // Re-queue the fiber safely AFTER it has dropped off the CPU registers
        if (f->state == FIBER_SUSPENDED) {
            PushFiber(f);
        }
    }
}

void InitFiberSys(int worker_threads, ThreadPriority priority) {
    g_fiber_system.worker_count = worker_threads > 0 ? worker_threads : 4;
    g_fiber_system.workers = (KThread**)DIESEL_MEM_ALLOC(sizeof(KThread*) * g_fiber_system.worker_count);
    g_fiber_system.head = NULL;
    g_fiber_system.tail = NULL;
    g_fiber_system.queue_mutex = InitKMutex(); // Instantiates native mutex OR emulated null stub
    g_fiber_system.running = true;

    for (int i = 0; i < g_fiber_system.worker_count; ++i) {
        g_fiber_system.workers[i] = CreateKThread(FiberSchedulerLoop, NULL);
        SetKThreadPriority(g_fiber_system.workers[i], priority);
        StartKThread(g_fiber_system.workers[i]);
    }
}

void ShutdownFiberSys(void) {
    g_fiber_system.running = false;
    for (int i = 0; i < g_fiber_system.worker_count; ++i) {
#ifndef DIESEL_INTERNAL_IMPL_EMULATED
        JoinKThread(g_fiber_system.workers[i]);
#endif
        DestroyKThread(g_fiber_system.workers[i]);
    }

    DestroyKMutex(g_fiber_system.queue_mutex);
    DIESEL_MEM_FREE(g_fiber_system.workers);
    g_fiber_system.workers = NULL;
    g_fiber_system.head = NULL;
    g_fiber_system.tail = NULL;
}

Fiber* CreateNewFiber(FiberWorker worker, void* user_data) {
    Fiber* f = (Fiber*)DIESEL_MEM_ALLOC(sizeof(Fiber));
    if (!f) return NULL;

    f->ctx.ID = (uintptr_t)f;
    f->ctx.user_data = user_data;
    f->worker = worker;
    f->priority = THREAD_PRIORITY_DEFAULT;
    f->state = FIBER_READY;
    f->next = NULL;
    f->os_fiber_handle = NULL;
    f->stack_ptr = NULL;

#ifndef DIESEL_INTERNAL_IMPL_EMULATED
#if defined(_WIN32) || defined(_WIN64)
    f->os_fiber_handle = CreateFiber(0, NativeFiberEntry, f);
#else 
    ucontext_t* uctx = (ucontext_t*)DIESEL_MEM_ALLOC(sizeof(ucontext_t));
    if (getcontext(uctx) == 0) {
        size_t stack_sz = 64 * 1024; 
        f->stack_ptr = DIESEL_MEM_ALLOC(stack_sz);
        uctx->uc_stack.ss_sp = f->stack_ptr;
        uctx->uc_stack.ss_size = stack_sz;
        uctx->uc_link = NULL;
        makecontext(uctx, NativeFiberEntryPOSIX, 0);
        f->os_fiber_handle = uctx;
    }
#endif
#endif

    PushFiber(f);
    return f;
}

void YieldFiber(void) {
#ifdef DIESEL_INTERNAL_IMPL_EMULATED
    if (g_emulated_current_fiber && g_emulated_current_fiber->state == FIBER_RUNNING) {
        g_emulated_current_fiber->state = FIBER_SUSPENDED;
        YieldKThread();
    }
#else
    if (t_current_running_fiber && t_scheduler_fiber) {
        Fiber* current = t_current_running_fiber;
        current->state = FIBER_SUSPENDED;
#if defined(_WIN32) || defined(_WIN64)
        SwitchToFiber(t_scheduler_fiber);
#else
        swapcontext((ucontext_t*)current->os_fiber_handle, (ucontext_t*)t_scheduler_fiber);
#endif
    } else {
        YieldKThread();
    }
#endif
}

void SleepFiber(int ms) {
#ifdef DIESEL_INTERNAL_IMPL_EMULATED
    if (g_emulated_current_fiber && g_emulated_current_fiber->state == FIBER_RUNNING) {
        g_emulated_current_fiber->state = FIBER_SUSPENDED;
        SleepKThread(ms);
    }
#else
#if defined(_WIN32) || defined(_WIN64)
    uint64_t start = GetTickCount64();
    while ((GetTickCount64() - start) < (uint64_t)ms) { YieldFiber(); }
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t start = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    while (1) {
        struct timespec curr;
        clock_gettime(CLOCK_MONOTONIC, &curr);
        uint64_t now = (curr.tv_sec * 1000) + (curr.tv_nsec / 1000000);
        if ((now - start) >= (uint64_t)ms) break;
        YieldFiber();
    }
#endif
#endif
}

void RunFiber(Fiber* f) {
    if (!f || f->state == FIBER_FINISHED) return;
    PushFiber(f);
}

void DestroyFiber(Fiber* f) {
    if (!f) return;
#ifndef DIESEL_INTERNAL_IMPL_EMULATED
#if defined(_WIN32) || defined(_WIN64)
    if (f->os_fiber_handle) DeleteFiber(f->os_fiber_handle);
#else
    if (f->os_fiber_handle) DIESEL_MEM_FREE(f->os_fiber_handle);
    if (f->stack_ptr) DIESEL_MEM_FREE(f->stack_ptr);
#endif
#endif
    DIESEL_MEM_FREE(f);
}

void JoinFiber(Fiber* f) {
    if (!f) return;
    while (f->state != FIBER_FINISHED) {
        YieldFiber();
    }
}

#ifdef __cplusplus
}
#endif

#else 
typedef struct Fiber Fiber;
typedef void (*FiberWorker)(struct FiberContext* ctx);

void InitFiberSys(int worker_threads, ThreadPriority priority);
void ShutdownFiberSys(void);
Fiber* CreateNewFiber(FiberWorker worker, void* user_data);
void RunFiber(Fiber* f);
void YieldFiber(void);
void SleepFiber(int ms);
void DestroyFiber(Fiber* f);
void JoinFiber(Fiber* f);
#endif

#endif // DIESEL_FIBER_H

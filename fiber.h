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

/* ---------------- Fiber Structs ---------------- */
typedef struct Fiber Fiber;
struct Fiber {
    FiberContext ctx;
    FiberWorker worker;
    ThreadPriority priority;
    bool finished;
    Fiber* next;
};

typedef struct FiberSystem {
    Fiber* run_queue;
    KThread** workers;
    int worker_count;
    bool running;
} FiberSystem;

FiberSystem g_fiber_system = {0};

/* ---------------- Cross-platform atomics ---------------- */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
void* FiberDatLoad(void* ptr) {
    return (void*)atomic_load_explicit((atomic_uintptr_t*)ptr, memory_order_acquire);
}
void FiberDatStore(void* ptr, void* val) {
    atomic_store_explicit((atomic_uintptr_t*)ptr, (uintptr_t)val, memory_order_release);
}
#elif defined(_MSC_VER)
#include <intrin.h>
void* FiberDatLoad(void* ptr) {
    return _InterlockedCompareExchangePointer((void* volatile*)ptr, NULL, NULL);
}
void FiberDatStore(void* ptr, void* val) {
    _InterlockedExchangePointer((void* volatile*)ptr, val);
}
#elif defined(__GNUC__) || defined(__clang__)
void* FiberDatLoad(void* ptr) {
    return __atomic_load_n((void* volatile*)ptr, __ATOMIC_ACQUIRE);
}
void FiberDatStore(void* ptr, void* val) {
    __atomic_store_n((void* volatile*)ptr, val, __ATOMIC_RELEASE);
}
#else
void* FiberDatLoad(void* ptr) { return *(void**)ptr; }
void FiberDatStore(void* ptr, void* val) { *(void**)ptr = val; }
#endif

/* ---------------- Atomic run queue ---------------- */
Fiber* PopFiber(void) {
    Fiber* f;
    do {
        f = FiberDatLoad(&g_fiber_system.run_queue);
        if (!f) return NULL;
    } while (!__sync_bool_compare_and_swap((Fiber**)&g_fiber_system.run_queue, f, f->next));
    return f;
}

void PushFiber(Fiber* f) {
    Fiber* head;
    do {
        head = FiberDatLoad(&g_fiber_system.run_queue);
        f->next = head;
    } while (!__sync_bool_compare_and_swap((Fiber**)&g_fiber_system.run_queue, head, f));
}

/* ---------------- Fiber Scheduler ---------------- */
void FiberSchedulerLoop(KThreadContext* _unused) {
    (void)_unused;
    while (g_fiber_system.running) {
        Fiber* f = PopFiber();
        if (!f) {
            SleepKThread(1);
            continue;
        }

        if (!f->finished) {
            f->worker(&f->ctx);
            f->finished = true;
        }
    }
}

/* ---------------- Fiber System API ---------------- */
void InitFiberSys(int worker_threads, ThreadPriority priority) {
    g_fiber_system.worker_count = worker_threads > 0 ? worker_threads : 4;
    g_fiber_system.workers = (KThread**)DIESEL_MEM_ALLOC(sizeof(KThread*) * g_fiber_system.worker_count);
    g_fiber_system.run_queue = NULL;
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
        JoinKThread(g_fiber_system.workers[i]);
        DestroyKThread(g_fiber_system.workers[i]);
    }
    DIESEL_MEM_FREE(g_fiber_system.workers);
    g_fiber_system.workers = NULL;
    g_fiber_system.run_queue = NULL;
}

Fiber* CreateNewFiber(FiberWorker worker, void* user_data) {
    Fiber* f = (Fiber*)DIESEL_MEM_ALLOC(sizeof(Fiber));
    if (!f) return NULL;

    f->ctx.ID = (uintptr_t)f;
    f->ctx.user_data = user_data;
    f->worker = worker;
    f->priority = THREAD_PRIORITY_DEFAULT;
    f->finished = false;
    f->next = NULL;

    PushFiber(f);
    return f;
}

void SetFiberPriority(Fiber* f, ThreadPriority priority) {
    if (!f) return;
    f->priority = priority;
}

void RunFiber(Fiber* f) {
    if (!f || f->finished) return;
    PushFiber(f);
}

void YieldFiber(void) {
    YieldKThread();
}

void SleepFiber(int ms) {
    SleepKThread(ms);
}

void DestroyFiber(Fiber* f) {
    if (!f) return;
    DIESEL_MEM_FREE(f);
}

void JoinFiber(Fiber* f) {
    if (!f) return;
    while (!f->finished) YieldFiber();
}

#ifdef __cplusplus
}
#endif

#else // Public API

/* ---------------- Public API ---------------- */
typedef struct Fiber Fiber;
typedef void (*FiberWorker)(struct FiberContext* ctx);

void* FiberDatLoad(void* ptr);
void FiberDatStore(void* ptr, void* val);
void InitFiberSys(int worker_threads, ThreadPriority priority);
void ShutdownFiberSys(void);
Fiber* CreateNewFiber(FiberWorker worker, void* user_data);
void SetFiberPriority(Fiber* f, ThreadPriority priority);
void RunFiber(Fiber* f);
void YieldFiber(void);
void SleepFiber(int ms);
void DestroyFiber(Fiber* f);
void JoinFiber(Fiber* f);

#endif // DIESEL_INTERNAL_IMPL_PLATFORM
#endif // DIESEL_FIBER_H

#ifndef DIESEL_PLATFORM_H
#define DIESEL_PLATFORM_H

/**
 * @file platform.h
 * @brief Diesel platform abstraction header.
 *
 * This file selects the correct platform-specific implementation
 * and manages the implementation chain when DIESEL_IMPLEMENTATION is defined.
 *
 * Mechanism:
 * 1. If `DIESEL_INTERNAL_IMPL_PLATFORM` is defined by diesel.h,
 *    this header will define one of the internal platform implementation macros:
 *    - DIESEL_INTERNAL_IMPL_WINDOWS
 *    - DIESEL_INTERNAL_IMPL_UNIX
 *    - DIESEL_INTERNAL_IMPL_EMULATED
 *
 * 2. Platform-specific headers can check these macros to include
 *    actual inline implementations.
*/
#ifdef DIESEL_INTERNAL_IMPL_PLATFORM

#if defined(_WIN32) || defined(_WIN64)
    #define DIESEL_INTERNAL_IMPL_WINDOWS
    #include "windows/kthread.h"
    #include "windows/ksync.h"

#elif defined(__unix__) || defined(__APPLE__)
    #define DIESEL_INTERNAL_IMPL_UNIX
    #include "unix/kthread.h"
    #include "unix/ksync.h"

#else
    #define DIESEL_INTERNAL_IMPL_EMULATED
    #include "emulated/kthread.h"
    #include "emulated/ksync.h"

#endif // platform selection

#else
#if defined(_WIN32) || defined(_WIN64)
    #include "windows/kthread.h"
    #include "windows/ksync.h"

#elif defined(__unix__) || defined(__APPLE__)
    #include "unix/kthread.h"
    #include "unix/ksync.h"

#else
    #include "emulated/kthread.h"
    #include "emulated/ksync.h"

#endif // platform include

#endif // DIESEL_INTERNAL_IMPL_PLATFORM

#endif // DIESEL_PLATFORM_H

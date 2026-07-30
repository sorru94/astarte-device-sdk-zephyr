/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_CLEANUP_H
#define ASTARTE_CLEANUP_H

/**
 * @file cleanup.h
 * @brief Polyfill for Zephyr >= 4.4.0 scope-based cleanup helpers.
 *
 * @details This allows compiling the SDK against Zephyr 4.3.0 and older
 * by recreating the macros using standard GCC/Clang attributes.
 */

#ifdef CONFIG_SCOPE_CLEANUP_HELPERS

// If Zephyr supports the cleanup helpers natively, use the official API
#include <zephyr/cleanup.h>

#define ASTARTE_SCOPE_DEFER_DEFINE(...) SCOPE_DEFER_DEFINE(__VA_ARGS__) // NOLINT

#else

/* ------------------------------------------------------------------------- *
 * FALLBACK MACROS FOR ZEPHYR < 4.4.0                                        *
 * ------------------------------------------------------------------------- */

/** @cond INTERNAL_HIDDEN */
// Utility macros to generate unique variable names using __COUNTER__
#define _ASTARTE_CONCAT_IMPL2(a, b) a##b
#define _ASTARTE_CONCAT_IMPL(a, b) _ASTARTE_CONCAT_IMPL2(a, b)
#define _ASTARTE_UNIQUE_ID(prefix) _ASTARTE_CONCAT_IMPL(prefix, __COUNTER__)

// Macro overloading magic to dynamically support 1 or 2 arguments
#define _ASTARTE_NUM_ARGS(...) _ASTARTE_NUM_ARGS_IMPL(__VA_ARGS__, 5, 4, 3, 2, 1, 0)
#define _ASTARTE_NUM_ARGS_IMPL(_1, _2, _3, _4, _5, N, ...) N

// Definition for 1 argument (Function takes NO parameters)
#define _SCOPE_DEFER_DEFINE_1(_func)                                                               \
    typedef int cleanup_defer_##_func##_t;                                                         \
    static inline void cleanup_defer_##_func##_exit(cleanup_defer_##_func##_t *val)                \
    {                                                                                              \
        (void) val;                                                                                \
        _func();                                                                                   \
    }                                                                                              \
    static inline cleanup_defer_##_func##_t cleanup_defer_##_func##_init(void)                     \
    {                                                                                              \
        return 0;                                                                                  \
    }

// Definition for 2 arguments (Function takes a typed parameter)
#define _SCOPE_DEFER_DEFINE_2(_func, _type)                                                        \
    typedef _type cleanup_defer_##_func##_t;                                                       \
    static inline void cleanup_defer_##_func##_exit(cleanup_defer_##_func##_t *val)                \
    {                                                                                              \
        if (*val) {                                                                                \
            _func(*val);                                                                           \
        }                                                                                          \
    }                                                                                              \
    static inline cleanup_defer_##_func##_t cleanup_defer_##_func##_init(_type val)                \
    {                                                                                              \
        return val;                                                                                \
    }

#define _SCOPE_DEFER_DEFINE_CHOOSER2(count) _SCOPE_DEFER_DEFINE_##count
#define _SCOPE_DEFER_DEFINE_CHOOSER1(count) _SCOPE_DEFER_DEFINE_CHOOSER2(count)
/** @endcond */

/**
 * @brief Polyfill for SCOPE_VAR_DEFINE
 */
#define SCOPE_VAR_DEFINE(_name, _type, _exit_fn, _init_fn, ...)                                    \
    typedef _type cleanup_##_name##_t;                                                             \
    static inline void cleanup_##_name##_exit(_type *val)                                          \
    {                                                                                              \
        _type _T = *val;                                                                           \
        (void) _T;                                                                                 \
        {                                                                                          \
            _exit_fn;                                                                              \
        }                                                                                          \
    }                                                                                              \
    static inline _type cleanup_##_name##_init(__VA_ARGS__)                                        \
    {                                                                                              \
        return (_type) (_init_fn);                                                                 \
    }

/**
 * @brief Polyfill for SCOPE_GUARD_DEFINE
 */
#define SCOPE_GUARD_DEFINE(_name, _type, _lock_expr, _unlock_expr)                                 \
    typedef _type cleanup_guard_##_name##_t;                                                       \
    static inline void cleanup_guard_##_name##_exit(cleanup_guard_##_name##_t *val)                \
    {                                                                                              \
        _type _T = *val;                                                                           \
        (void) _T;                                                                                 \
        {                                                                                          \
            _unlock_expr;                                                                          \
        }                                                                                          \
    }                                                                                              \
    static inline cleanup_guard_##_name##_t cleanup_guard_##_name##_init(_type _T)                 \
    {                                                                                              \
        {                                                                                          \
            _lock_expr;                                                                            \
        }                                                                                          \
        return _T;                                                                                 \
    }

/**
 * @brief Polyfill for ASTARTE_SCOPE_DEFER_DEFINE (Supports 1 or 2 arguments)
 */
#define ASTARTE_SCOPE_DEFER_DEFINE(...)                                                            \
    _SCOPE_DEFER_DEFINE_CHOOSER1(_ASTARTE_NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

/**
 * @brief Polyfill for scope_var
 */
#define scope_var(_name, _var)                                                                     \
    cleanup_##_name##_t _var __attribute__((__cleanup__(cleanup_##_name##_exit)))                  \
    = cleanup_##_name##_init

/**
 * @brief Polyfill for scope_var_init
 */
#define scope_var_init(_name, _var, _init_expr)                                                    \
    cleanup_##_name##_t _var __attribute__((__cleanup__(cleanup_##_name##_exit))) = (_init_expr)

/**
 * @brief Polyfill for scope_defer
 */
#define scope_defer(_name)                                                                         \
    cleanup_defer_##_name##_t _ASTARTE_UNIQUE_ID(_defer_)                                          \
        __attribute__((__cleanup__(cleanup_defer_##_name##_exit))) = cleanup_defer_##_name##_init

/**
 * @brief Polyfill for scope_guard
 */
#define scope_guard(_name)                                                                         \
    cleanup_guard_##_name##_t _ASTARTE_UNIQUE_ID(_guard_)                                          \
        __attribute__((__cleanup__(cleanup_guard_##_name##_exit))) = cleanup_guard_##_name##_init

#endif // CONFIG_SCOPE_CLEANUP_HELPERS

#endif // ASTARTE_CLEANUP_H

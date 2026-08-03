/*
 * Copyright (c) 2026 Hazel Wireless
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HAZEL_INCLUDE_SYS_ATOMIC_GCC_H_
#define HAZEL_INCLUDE_SYS_ATOMIC_GCC_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These typedefs are expected to match the public atomic.h.
 * Remove them if already defined there.
 */
typedef int32_t  atomic_t;
typedef int32_t  atomic_val_t;

typedef intptr_t atomic_ptr_t;
typedef void    *atomic_ptr_val_t;

/*--------------------------------------------------------------------------*/
/* Memory barriers                                                          */
/*--------------------------------------------------------------------------*/

static inline void atomic_thread_fence(void)
{
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

static inline void atomic_signal_fence(void)
{
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
}

/*--------------------------------------------------------------------------*/
/* Load / Store                                                             */
/*--------------------------------------------------------------------------*/

static inline atomic_val_t atomic_get(const atomic_t *target)
{
    return __atomic_load_n(target, __ATOMIC_SEQ_CST);
}

static inline atomic_ptr_val_t atomic_ptr_get(const atomic_ptr_t *target)
{
    return (atomic_ptr_val_t)__atomic_load_n(target, __ATOMIC_SEQ_CST);
}

static inline atomic_val_t atomic_set(atomic_t *target,
                                      atomic_val_t value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

static inline atomic_ptr_val_t atomic_ptr_set(atomic_ptr_t *target,
                                              atomic_ptr_val_t value)
{
    return (atomic_ptr_val_t)
        __atomic_exchange_n(target, (intptr_t)value, __ATOMIC_SEQ_CST);
}

static inline atomic_val_t atomic_clear(atomic_t *target)
{
    return atomic_set(target, 0);
}

static inline atomic_ptr_val_t atomic_ptr_clear(atomic_ptr_t *target)
{
    return atomic_ptr_set(target, NULL);
}

/*--------------------------------------------------------------------------*/
/* Compare Exchange                                                         */
/*--------------------------------------------------------------------------*/

static inline bool atomic_cas(atomic_t *target,
                              atomic_val_t old_value,
                              atomic_val_t new_value)
{
    return __atomic_compare_exchange_n(target,
                                       &old_value,
                                       new_value,
                                       false,
                                       __ATOMIC_SEQ_CST,
                                       __ATOMIC_SEQ_CST);
}

static inline bool atomic_ptr_cas(atomic_ptr_t *target,
                                  atomic_ptr_val_t old_value,
                                  atomic_ptr_val_t new_value)
{
    intptr_t expected = (intptr_t)old_value;

    return __atomic_compare_exchange_n(target,
                                       &expected,
                                       (intptr_t)new_value,
                                       false,
                                       __ATOMIC_SEQ_CST,
                                       __ATOMIC_SEQ_CST);
}

/*--------------------------------------------------------------------------*/
/* Arithmetic                                                               */
/*--------------------------------------------------------------------------*/

static inline atomic_val_t atomic_add(atomic_t *target,
                                      atomic_val_t value)
{
    return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST);
}

static inline atomic_val_t atomic_sub(atomic_t *target,
                                      atomic_val_t value)
{
    return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST);
}

static inline atomic_val_t atomic_inc(atomic_t *target)
{
    return __atomic_fetch_add(target, 1, __ATOMIC_SEQ_CST);
}

static inline atomic_val_t atomic_dec(atomic_t *target)
{
    return __atomic_fetch_sub(target, 1, __ATOMIC_SEQ_CST);
}

/*--------------------------------------------------------------------------*/
/* Bit operations                                                           */
/*--------------------------------------------------------------------------*/

static inline atomic_val_t atomic_or(atomic_t *target,
                                     atomic_val_t value)
{
    return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST);
}

static inline atomic_val_t atomic_xor(atomic_t *target,
                                      atomic_val_t value)
{
    return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST);
}

static inline atomic_val_t atomic_and(atomic_t *target,
                                      atomic_val_t value)
{
    return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST);
}

/*--------------------------------------------------------------------------*/
/* NAND                                                                     */
/*--------------------------------------------------------------------------*/

static inline atomic_val_t atomic_nand(atomic_t *target,
                                       atomic_val_t value)
{
    atomic_val_t old;
    atomic_val_t new_val;

    do {
        old = __atomic_load_n(target, __ATOMIC_SEQ_CST);
        new_val = ~(old & value);
    } while (!__atomic_compare_exchange_n(target,
                                          &old,
                                          new_val,
                                          false,
                                          __ATOMIC_SEQ_CST,
                                          __ATOMIC_SEQ_CST));

    return old;
}

static inline atomic_val_t atomic_get_relaxed(const atomic_t *target)
{
    return __atomic_load_n(target, __ATOMIC_RELAXED);
}

static inline atomic_val_t atomic_get_acquire(const atomic_t *target)
{
    return __atomic_load_n(target, __ATOMIC_ACQUIRE);
}

static inline void atomic_set_release(atomic_t *target, atomic_val_t value)
{
    __atomic_store_n(target, value, __ATOMIC_RELEASE);
}

#ifdef __cplusplus
}
#endif

#endif /* HAZEL_INCLUDE_SYS_ATOMIC_GCC_H_ */

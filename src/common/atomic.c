/**
 * @file atomic.c
 * @brief Sequentially consistent atomic operations for non-Zephyr backends.
 *
 * GCC __atomic builtins map to the target architecture's native atomic
 * instructions and provide the full memory barrier required by the Zephyr
 * atomic API. Zephyr builds use their native implementation instead.
 */
#include <hazel_wireless/sys/atomic.h>

bool atomic_cas(atomic_t *target, atomic_val_t old_value,
                atomic_val_t new_value)
{
    return __atomic_compare_exchange_n(target, &old_value, new_value, false,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

bool atomic_ptr_cas(atomic_ptr_t *target, atomic_ptr_val_t old_value,
                    atomic_ptr_val_t new_value)
{
    return __atomic_compare_exchange_n(target, &old_value, new_value, false,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

atomic_val_t atomic_add(atomic_t *target, atomic_val_t value)
{
    return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST);
}

atomic_val_t atomic_sub(atomic_t *target, atomic_val_t value)
{
    return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST);
}

atomic_val_t atomic_inc(atomic_t *target)
{
    return atomic_add(target, 1);
}

atomic_val_t atomic_dec(atomic_t *target)
{
    return atomic_sub(target, 1);
}

atomic_val_t atomic_get(const atomic_t *target)
{
    return __atomic_load_n(target, __ATOMIC_SEQ_CST);
}

atomic_ptr_val_t atomic_ptr_get(const atomic_ptr_t *target)
{
    return __atomic_load_n(target, __ATOMIC_SEQ_CST);
}

atomic_val_t atomic_set(atomic_t *target, atomic_val_t value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

atomic_ptr_val_t atomic_ptr_set(atomic_ptr_t *target,
                                atomic_ptr_val_t value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

atomic_val_t atomic_clear(atomic_t *target)
{
    return atomic_set(target, 0);
}

atomic_ptr_val_t atomic_ptr_clear(atomic_ptr_t *target)
{
    return atomic_ptr_set(target, NULL);
}

atomic_val_t atomic_or(atomic_t *target, atomic_val_t value)
{
    return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST);
}

atomic_val_t atomic_xor(atomic_t *target, atomic_val_t value)
{
    return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST);
}

atomic_val_t atomic_and(atomic_t *target, atomic_val_t value)
{
    return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST);
}

atomic_val_t atomic_nand(atomic_t *target, atomic_val_t value)
{
    return __atomic_fetch_nand(target, value, __ATOMIC_SEQ_CST);
}

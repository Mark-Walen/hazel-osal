/**
 * @file atomic_arch.h
 * @brief External atomic primitive declarations for standalone OSAL builds.
 *
 * The implementation is provided by src/common/atomic.c. Applications should
 * normally include hazel_wireless/sys/atomic.h instead of this file directly.
 */
#ifndef HAZEL_INCLUDE_SYS_ATOMIC_ARCH_H_
#define HAZEL_INCLUDE_SYS_ATOMIC_ARCH_H_

#include <stdbool.h>
#include <hazel_wireless/sys/atomic_types.h>

/* Included from <atomic.h> */

/* Arch specific atomic primitives */

bool atomic_cas(atomic_t *target, atomic_val_t old_value,
			 atomic_val_t new_value);

bool atomic_ptr_cas(atomic_ptr_t *target, atomic_ptr_val_t old_value,
		    atomic_ptr_val_t new_value);

atomic_val_t atomic_add(atomic_t *target, atomic_val_t value);

atomic_val_t atomic_sub(atomic_t *target, atomic_val_t value);

atomic_val_t atomic_inc(atomic_t *target);

atomic_val_t atomic_dec(atomic_t *target);

atomic_val_t atomic_get(const atomic_t *target);

void *atomic_ptr_get(const atomic_ptr_t *target);

atomic_val_t atomic_set(atomic_t *target, atomic_val_t value);

atomic_ptr_val_t atomic_ptr_set(atomic_ptr_t *target, atomic_ptr_val_t value);

atomic_val_t atomic_clear(atomic_t *target);

void *atomic_ptr_clear(atomic_ptr_t *target);

atomic_val_t atomic_or(atomic_t *target, atomic_val_t value);

atomic_val_t atomic_xor(atomic_t *target, atomic_val_t value);

atomic_val_t atomic_and(atomic_t *target, atomic_val_t value);

atomic_val_t atomic_nand(atomic_t *target, atomic_val_t value);

#endif /* HAZEL_INCLUDE_SYS_ATOMIC_ARCH_H_ */

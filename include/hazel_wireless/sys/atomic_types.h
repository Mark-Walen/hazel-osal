/**
 * @file atomic_types.h
 * @brief Machine-word integer and pointer types used by the atomic API.
 */
#ifndef HAZEL_INCLUDE_SYS_ATOMIC_TYPES_H_
#define HAZEL_INCLUDE_SYS_ATOMIC_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/** Atomic integer storage; one machine word on supported targets. */
typedef long atomic_t;
/** Value passed to and returned from atomic integer operations. */
typedef atomic_t atomic_val_t;
/** Atomic pointer storage. */
typedef void *atomic_ptr_t;
/** Value passed to and returned from atomic pointer operations. */
typedef atomic_ptr_t atomic_ptr_val_t;

#ifdef __cplusplus
}
#endif

#endif /* HAZEL_INCLUDE_SYS_ATOMIC_TYPES_H_ */

#ifndef LYNX_INCLUDE_SYS_ATOMIC_TYPES_H_
#define LYNX_INCLUDE_SYS_ATOMIC_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef long atomic_t;
typedef atomic_t atomic_val_t;
typedef void *atomic_ptr_t;
typedef atomic_ptr_t atomic_ptr_val_t;

#ifdef __cplusplus
}
#endif

#endif /* LYNX_INCLUDE_SYS_ATOMIC_TYPES_H_ */

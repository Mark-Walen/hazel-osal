/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Memory slab: fixed-size block allocator with an intrusive free list.
 * Zephyr-style API in the sys/ namespace; algorithm ported from Zephyr's
 * k_mem_slab (heap-free, deterministic, O(1) alloc/free).
 */

#ifndef LYNX_INCLUDE_SYS_MEM_SLAB_H_
#define LYNX_INCLUDE_SYS_MEM_SLAB_H_

#include <stddef.h>
#include <stdint.h>
#include <lynx_wireless/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @brief Memory slab API (fixed-size block allocator)
 *
 * @defgroup sys_mem_slab Memory Slab API
 * @ingroup os_services
 * @{
 *
 * A @ref sys_mem_slab is a Zephyr-style memory slab: a statically provided
 * buffer partitioned into equal-sized blocks. Allocation and freeing are O(1)
 * through an intrusive singly-linked free list stored in the first word of
 * each free block, so every block is rounded up to pointer alignment and must
 * be at least @c sizeof(void *) bytes.
 *
 * This is the deterministic, heap-free slab (the k_mem_slab algorithm) exposed
 * in the @c sys/ namespace. It has no kernel dependency: allocation never
 * blocks and returns -ENOMEM when the pool is exhausted.
 *
 * @note Not thread-safe. If a slab is shared between tasks/ISRs, the caller
 *       must serialize all access (same convention as sys_slist).
 *
 * @note Blocks handed to sys_mem_slab_free() must have been obtained from
 *       sys_mem_slab_alloc(); freeing an unallocated or already-freed block
 *       is undefined.
 */

/** Default backing-buffer alignment (power of two). */
#define SYS_MEM_SLAB_ALIGN sizeof(void *)

/**
 * @brief Compute the backing-buffer size for a slab.
 *
 * @param slab_block_size  Size of each block in bytes.
 * @param slab_num_blocks  Number of blocks.
 *
 * @return Number of bytes required (block size rounded up to pointer
 *         alignment).
 */
#define SYS_MEM_SLAB_BUFFER_SIZE(slab_block_size, slab_num_blocks) \
	((size_t)(WB_UP(slab_block_size) * (slab_num_blocks)))

/** Memory slab control block. */
struct sys_mem_slab {
	/** Total number of fixed-size blocks in the pool. */
	uint32_t num_blocks;
	/** Size of each block in bytes (rounded up to pointer alignment). */
	size_t block_size;
	/** Start of the backing buffer. */
	char *buffer;
	/** Head of the intrusive free list (NULL when fully allocated). */
	char *free_list;
	/** Number of blocks currently allocated. */
	uint32_t num_used;
};

/**
 * @brief Initialize a slab backed by a caller-provided buffer.
 *
 * @param slab        Slab to initialize.
 * @param buffer      Backing storage, aligned to @ref SYS_MEM_SLAB_ALIGN and
 *                    at least @ref SYS_MEM_SLAB_BUFFER_SIZE bytes.
 * @param block_size  Size of each block in bytes. Rounded up to a multiple of
 *                    @c sizeof(void *); must be non-zero.
 * @param num_blocks  Number of blocks @p buffer can hold; must be non-zero.
 *
 * @retval 0        Success.
 * @retval -EINVAL  Invalid argument.
 */
int sys_mem_slab_init(struct sys_mem_slab *slab, void *buffer,
		      size_t block_size, uint32_t num_blocks);

/**
 * @brief Allocate one block (non-blocking).
 *
 * On first use of a slab declared with SYS_MEM_SLAB_DEFINE() (which has no
 * boot-time initialization hook in this environment), the free list is built
 * lazily.
 *
 * @param slab  Slab to allocate from.
 * @param mem   Where to store the pointer to the allocated block.
 *
 * @retval 0        Success; @p *mem points at the block.
 * @retval -EINVAL  Invalid argument or unconfigured slab.
 * @retval -ENOMEM  No free blocks.
 */
int sys_mem_slab_alloc(struct sys_mem_slab *slab, void **mem);

/**
 * @brief Return a block to the slab.
 *
 * @param slab  Slab the block belongs to.
 * @param mem   Block to free (NULL is ignored). Must originate from
 *              sys_mem_slab_alloc() and not already be freed.
 */
void sys_mem_slab_free(struct sys_mem_slab *slab, void *mem);

/** @brief Number of blocks currently allocated. */
uint32_t sys_mem_slab_num_used_get(const struct sys_mem_slab *slab);

/** @brief Number of blocks currently available. */
uint32_t sys_mem_slab_num_free_get(const struct sys_mem_slab *slab);

/*
 * Compiler portability shims. The Zephyr toolchain __aligned/__used helpers
 * are not provided in this environment, so define local equivalents for the
 * DEFINE macros below. On non-GCC/clang hosts (e.g. MSVC test builds) the
 * alignment attribute is a no-op; x86 tolerates the resulting unaligned free
 * list accesses, and production firmware is always built with GCC.
 */
#if defined(__GNUC__) || defined(__clang__)
#define __sys_mem_slab_aligned(x) __attribute__((aligned(x)))
#define __sys_mem_slab_used      __attribute__((__used__))
#else
#define __sys_mem_slab_aligned(x)
#define __sys_mem_slab_used
#endif

/**
 * @brief Static initializer for a @ref sys_mem_slab.
 *
 * For a slab declared with SYS_MEM_SLAB_DEFINE() the free list is built on
 * first allocation; sys_mem_slab_init() is not required.
 */
#define SYS_MEM_SLAB_INITIALIZER(_buffer, _block_size, _num_blocks) \
	{ \
		.num_blocks = (_num_blocks), \
		.block_size = WB_UP(_block_size), \
		.buffer = (char *)(_buffer), \
		.free_list = NULL, \
		.num_used = 0U, \
	}

/**
 * @brief Define a memory slab with global storage.
 *
 * Declares the backing buffer and the slab control block. The slab is usable
 * without an explicit sys_mem_slab_init() call: the free list is built on the
 * first allocation.
 *
 * @param name             Slab variable name.
 * @param slab_block_size  Size of each block in bytes.
 * @param slab_num_blocks  Number of blocks.
 * @param slab_align       Backing-buffer alignment (positive power of two).
 */
#define SYS_MEM_SLAB_DEFINE(name, slab_block_size, slab_num_blocks, slab_align) \
	__sys_mem_slab_aligned(WB_UP(slab_align)) \
		char _sys_mem_slab_buf_##name[SYS_MEM_SLAB_BUFFER_SIZE(slab_block_size, \
								       slab_num_blocks)]; \
	struct sys_mem_slab name __sys_mem_slab_used = \
		SYS_MEM_SLAB_INITIALIZER(_sys_mem_slab_buf_##name, \
					 slab_block_size, slab_num_blocks)

/**
 * @brief Define a memory slab with static (file-local) storage.
 *
 * @copydetails SYS_MEM_SLAB_DEFINE()
 */
#define SYS_MEM_SLAB_DEFINE_STATIC(name, slab_block_size, slab_num_blocks, \
				   slab_align) \
	static __sys_mem_slab_aligned(WB_UP(slab_align)) \
		char _sys_mem_slab_buf_##name[SYS_MEM_SLAB_BUFFER_SIZE(slab_block_size, \
								       slab_num_blocks)]; \
	static struct sys_mem_slab name __sys_mem_slab_used = \
		SYS_MEM_SLAB_INITIALIZER(_sys_mem_slab_buf_##name, \
					 slab_block_size, slab_num_blocks)

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* LYNX_INCLUDE_SYS_MEM_SLAB_H_ */

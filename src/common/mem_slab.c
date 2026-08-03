/**
 * @file mem_slab.c
 * @brief Fixed-size block allocator over caller-owned storage.
 */
#include <lynx_wireless/kernel.h>

#include "internal.h"

int os_mem_slab_init(struct os_mem_slab *slab, void *buffer,
                     size_t block_size, uint32_t num_blocks)
{
    uint32_t i;

    if (!slab || !buffer || block_size < sizeof(void *) || num_blocks == 0U ||
        ((uintptr_t)buffer % sizeof(void *)) != 0U ||
        (block_size % sizeof(void *)) != 0U ||
        block_size > (SIZE_MAX / num_blocks)) {
        return -EINVAL;
    }
    os_common_zero(slab, sizeof(*slab));
    for (i = 0; i < num_blocks; ++i) {
        uint8_t *block = (uint8_t *)buffer + i * block_size;
        void *next = (i + 1U < num_blocks) ? block + block_size : NULL;

        *(void **)block = next;
    }
    if (os_sem_init(&slab->available, num_blocks, num_blocks) != 0) {
        return -ENOMEM;
    }
    slab->buffer = buffer;
    slab->free_list = buffer;
    slab->block_size = block_size;
    slab->num_blocks = num_blocks;
    slab->free_blocks = num_blocks;
    slab->initialized = true;
    return 0;
}

int os_mem_slab_deinit(struct os_mem_slab *slab)
{
    int result;

    if (!slab || !slab->initialized) {
        return -EINVAL;
    }
    if (os_mem_slab_num_free_get(slab) != slab->num_blocks) {
        return -EBUSY;
    }
    result = os_sem_deinit(&slab->available);
    if (result == 0) {
        os_common_zero(slab, sizeof(*slab));
    }
    return result;
}

int os_mem_slab_alloc(struct os_mem_slab *slab, void **memory,
                      uint32_t timeout)
{
    os_critical_key_t key;
    int result;

    if (!slab || !slab->initialized || !memory) {
        return -EINVAL;
    }
    result = os_sem_take(&slab->available, timeout);
    if (result != 0) {
        return result;
    }
    key = os_enter_critical();
    *memory = slab->free_list;
    slab->free_list = *(void **)slab->free_list;
    --slab->free_blocks;
    os_exit_critical(key);
    return 0;
}

int os_mem_slab_free(struct os_mem_slab *slab, void *memory)
{
    void *free_block;
    uint32_t scanned = 0;
    uintptr_t start;
    uintptr_t address;
    size_t total;
    os_critical_key_t key;

    if (!slab || !slab->initialized || !memory) {
        return -EINVAL;
    }
    start = (uintptr_t)slab->buffer;
    address = (uintptr_t)memory;
    total = slab->block_size * slab->num_blocks;
    if (address < start || (address - start) >= total ||
        ((address - start) % slab->block_size) != 0U) {
        return -EINVAL;
    }
    key = os_enter_critical();
    if (slab->free_blocks >= slab->num_blocks) {
        os_exit_critical(key);
        return -EINVAL;
    }
    free_block = slab->free_list;
    while (free_block != NULL && scanned++ < slab->free_blocks) {
        if (free_block == memory) {
            os_exit_critical(key);
            return -EINVAL;
        }
        free_block = *(void **)free_block;
    }
    *(void **)memory = slab->free_list;
    slab->free_list = memory;
    ++slab->free_blocks;
    os_exit_critical(key);
    os_sem_give(&slab->available);
    return 0;
}

uint32_t os_mem_slab_num_free_get(struct os_mem_slab *slab)
{
    uint32_t count;
    os_critical_key_t key;

    if (!slab || !slab->initialized) {
        return 0U;
    }
    key = os_enter_critical();
    count = slab->free_blocks;
    os_exit_critical(key);
    return count;
}

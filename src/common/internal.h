#ifndef LYNX_OSAL_COMMON_INTERNAL_H
#define LYNX_OSAL_COMMON_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

static inline void os_common_zero(void *memory, size_t size)
{
    uint8_t *bytes = memory;

    while (size-- != 0U) {
        *bytes++ = 0U;
    }
}

static inline void os_common_copy(void *destination, const void *source,
                                  size_t size)
{
    uint8_t *output = destination;
    const uint8_t *input = source;

    while (size-- != 0U) {
        *output++ = *input++;
    }
}

#endif

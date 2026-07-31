#include "internal.h"

void *os_malloc(size_t size)
{
    if ((size == 0U) || os_is_in_isr()) {
        return NULL;
    }
    return pvPortMalloc(size);
}

void os_free(void *ptr)
{
    if ((ptr != NULL) && !os_is_in_isr()) {
        vPortFree(ptr);
    }
}

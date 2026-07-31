#include "internal.h"

void *os_malloc(size_t size)
{
#ifdef RT_USING_HEAP
    if ((size != 0U) && !os_is_in_isr()) {
        return rt_malloc((rt_size_t)size);
    }
#else
    (void)size;
#endif
    return NULL;
}

void os_free(void *ptr)
{
#ifdef RT_USING_HEAP
    if ((ptr != NULL) && !os_is_in_isr()) {
        rt_free(ptr);
    }
#else
    (void)ptr;
#endif
}

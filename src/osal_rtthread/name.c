#include "internal.h"

static rt_uint32_t os_rt_name_sequence;

static rt_uint32_t os_rt_hash_mix(rt_uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

void os_rt_name_generate(char name[RT_NAME_MAX], const char *kind,
                         const void *object, const char *salt)
{
    rt_base_t level;
    rt_ubase_t address = (rt_ubase_t)object;
    rt_uint32_t hash;

    level = rt_hw_interrupt_disable();
    hash = ++os_rt_name_sequence;
    rt_hw_interrupt_enable(level);

    hash ^= (rt_uint32_t)address;
#if UINTPTR_MAX > UINT32_MAX
    hash ^= (rt_uint32_t)(address >> 32);
#endif
    hash ^= (rt_uint32_t)rt_tick_get();
    while (salt != RT_NULL && *salt != '\0') {
        hash = (hash ^ (rt_uint8_t)*salt++) * 16777619U;
    }
    hash = os_rt_hash_mix(hash);

#if RT_NAME_MAX >= 20
    (void)rt_snprintf(name, RT_NAME_MAX, "osal-%s-%08x", kind, hash);
#elif RT_NAME_MAX >= 10
    (void)rt_snprintf(name, RT_NAME_MAX, "o%08x", hash);
#else
    (void)rt_snprintf(name, RT_NAME_MAX, "%08x", hash);
#endif
}

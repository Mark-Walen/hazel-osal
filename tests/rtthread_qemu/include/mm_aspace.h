#ifndef OSAL_TEST_MM_ASPACE_H
#define OSAL_TEST_MM_ASPACE_H

#include <rtthread.h>

struct rt_aspace;
struct rt_varea {
    rt_ubase_t unused;
};

typedef struct rt_aspace *rt_aspace_t;

enum rt_mmu_cntl {
    RT_MMU_CNTL_DUMMY = 0,
};

#endif

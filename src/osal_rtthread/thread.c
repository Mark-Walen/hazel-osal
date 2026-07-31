#include "internal.h"

#include <string.h>

#define OS_RT_THREAD_SLICE 10U

static void os_rt_thread_entry(void *parameter)
{
    struct os_thread *thread = (struct os_thread *)parameter;
    thread->entry(thread->p1, thread->p2, thread->p3);
}

int os_thread_create(struct os_thread *thread,
                     const char *const thread_name,
                     os_thread_stack_t *stack, size_t stack_size,
                     os_thread_entry_t entry,
                     void *p1, void *p2, void *p3,
                     int prio, uint32_t options)
{
    rt_thread_t handle;
    rt_err_t result;

    (void)options;
    if (!thread || !thread_name || !entry || stack_size == 0U) {
        return -EINVAL;
    }

    memset(thread, 0, sizeof(*thread));
    thread->entry = entry;
    thread->p1 = p1;
    thread->p2 = p2;
    thread->p3 = p3;
    thread->stack = stack;
    thread->stack_size = stack_size;
    thread->state = OS_THREAD_READY;
    thread->base_priority = prio;
    sys_dlist_init(&thread->owned_mutexes);

    result = rt_sem_init(&thread->wait_storage, "osal-w", 0,
                         RT_IPC_FLAG_PRIO);
    if (result != RT_EOK) {
        return -ENOMEM;
    }
    (void)rt_sem_control(&thread->wait_storage, RT_IPC_CMD_SET_VLIMIT,
                         (void *)(rt_ubase_t)1U);

    if (stack != NULL) {
        result = rt_thread_init(&thread->tcb, thread_name,
                                os_rt_thread_entry, thread,
                                stack, (rt_uint32_t)stack_size,
                                os_to_rt_priority(prio),
                                OS_RT_THREAD_SLICE);
        if (result != RT_EOK) {
            return -ENOMEM;
        }
        handle = &thread->tcb;
    } else {
#ifdef RT_USING_HEAP
        handle = rt_thread_create(thread_name, os_rt_thread_entry, thread,
                                  (rt_uint32_t)stack_size,
                                  os_to_rt_priority(prio),
                                  OS_RT_THREAD_SLICE);
        if (handle == RT_NULL) {
            return -ENOMEM;
        }
#else
        return -ENOMEM;
#endif
    }

    thread->handle = handle;
    handle->user_data = (rt_ubase_t)thread;
    if (rt_thread_startup(handle) != RT_EOK) {
        return -EINVAL;
    }
    return 0;
}

struct os_thread *os_get_current_thread(void)
{
    rt_thread_t thread = rt_thread_self();

    return (thread == RT_NULL) ?
           NULL : (struct os_thread *)(rt_ubase_t)thread->user_data;
}

int os_thread_get_priority(struct os_thread *thread)
{
    rt_thread_t handle;

    if (!thread || !thread->handle) {
        return -EINVAL;
    }
    handle = (rt_thread_t)thread->handle;
    return rt_to_os_priority(RT_SCHED_PRIV(handle).current_priority);
}

void os_thread_set_priority(struct os_thread *thread, int priority)
{
    rt_uint8_t native_priority;

    if (!thread || !thread->handle) {
        return;
    }
    native_priority = os_to_rt_priority(priority);
    thread->base_priority = priority;
    (void)rt_thread_control((rt_thread_t)thread->handle,
                            RT_THREAD_CTRL_CHANGE_PRIORITY,
                            &native_priority);
}

void os_thread_yield(void)
{
    if (!os_is_in_isr()) {
        (void)rt_thread_yield();
    }
}

#include "internal.h"

#define OS_RT_THREAD_SLICE 10U

static void os_rt_thread_entry(void *parameter)
{
    struct os_thread *thread = (struct os_thread *)parameter;
    os_critical_key_t key;

    thread->entry(thread->p1, thread->p2, thread->p3);
    key = os_enter_critical();
    thread->completed = true;
    thread->state = OS_THREAD_TERMINATED;
    os_exit_critical(key);
    (void)rt_sem_release((rt_sem_t)thread->completion_handle);
    key = os_enter_critical();
    thread->completion_signaled = true;
    os_exit_critical(key);
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
    char native_name[RT_NAME_MAX];
    char wait_name[RT_NAME_MAX];
    char completion_name[RT_NAME_MAX];

    (void)options;
    if (!thread || !thread_name || !entry || stack_size == 0U) {
        return -EINVAL;
    }

    rt_memset(thread, 0, sizeof(*thread));
    thread->entry = entry;
    thread->p1 = p1;
    thread->p2 = p2;
    thread->p3 = p3;
    thread->stack = stack;
    thread->stack_size = stack_size;
    thread->state = OS_THREAD_READY;
    thread->base_priority = prio;
    sys_dlist_init(&thread->owned_mutexes);
    os_rt_name_generate(native_name, "thr", thread, thread_name);
    os_rt_name_generate(wait_name, "wait", &thread->wait_storage, thread_name);
    os_rt_name_generate(completion_name, "done",
                        &thread->completion_storage, thread_name);

    result = rt_sem_init(&thread->wait_storage, wait_name, 0,
                         RT_IPC_FLAG_PRIO);
    if (result != RT_EOK) {
        return -ENOMEM;
    }
    (void)rt_sem_control(&thread->wait_storage, RT_IPC_CMD_SET_VLIMIT,
                         (void *)(rt_ubase_t)1U);
    result = rt_sem_init(&thread->completion_storage, completion_name, 0,
                         RT_IPC_FLAG_PRIO);
    if (result != RT_EOK) {
        (void)rt_sem_detach(&thread->wait_storage);
        return -ENOMEM;
    }
    (void)rt_sem_control(&thread->completion_storage, RT_IPC_CMD_SET_VLIMIT,
                         (void *)(rt_ubase_t)1U);
    thread->completion_handle = &thread->completion_storage;

    if (stack != NULL) {
        result = rt_thread_init(&thread->tcb, native_name,
                                os_rt_thread_entry, thread,
                                stack, (rt_uint32_t)stack_size,
                                os_to_rt_priority(prio),
                                OS_RT_THREAD_SLICE);
        if (result != RT_EOK) {
            (void)rt_sem_detach(&thread->completion_storage);
            (void)rt_sem_detach(&thread->wait_storage);
            return -ENOMEM;
        }
        handle = &thread->tcb;
    } else {
#ifdef RT_USING_HEAP
        handle = rt_thread_create(native_name, os_rt_thread_entry, thread,
                                  (rt_uint32_t)stack_size,
                                  os_to_rt_priority(prio),
                                  OS_RT_THREAD_SLICE);
        if (handle == RT_NULL) {
            (void)rt_sem_detach(&thread->completion_storage);
            (void)rt_sem_detach(&thread->wait_storage);
            return -ENOMEM;
        }
#else
        (void)rt_sem_detach(&thread->completion_storage);
        (void)rt_sem_detach(&thread->wait_storage);
        return -ENOMEM;
#endif
    }

    thread->handle = handle;
    handle->user_data = (rt_ubase_t)thread;
    if (rt_thread_startup(handle) != RT_EOK) {
        if (stack != RT_NULL) {
            (void)rt_thread_detach(handle);
        } else {
#ifdef RT_USING_HEAP
            (void)rt_thread_delete(handle);
#endif
        }
        thread->handle = RT_NULL;
        (void)rt_sem_detach(&thread->completion_storage);
        (void)rt_sem_detach(&thread->wait_storage);
        thread->completion_handle = RT_NULL;
        return -EINVAL;
    }
    return 0;
}

static void os_rt_thread_reap(struct os_thread *thread)
{
    os_critical_key_t key;

    (void)rt_sem_detach(&thread->completion_storage);
    (void)rt_sem_detach(&thread->wait_storage);

    key = os_enter_critical();
    thread->completion_handle = RT_NULL;
    thread->handle = RT_NULL;
    thread->completion_reaped = true;
    thread->join_active = false;
    os_exit_critical(key);
}

int os_thread_join(struct os_thread *thread, uint32_t timeout)
{
    rt_err_t result;
    os_critical_key_t key;

    if (!thread || os_is_in_isr()) {
        return !thread ? -EINVAL : -EWOULDBLOCK;
    }
    if (os_get_current_thread() == thread) {
        return -EDEADLK;
    }

    key = os_enter_critical();
    if (thread->completion_reaped) {
        os_exit_critical(key);
        return 0;
    }
    if (!thread->completion_handle) {
        os_exit_critical(key);
        return -EINVAL;
    }
    if (thread->join_active) {
        os_exit_critical(key);
        return -EBUSY;
    }
    thread->join_active = true;
    os_exit_critical(key);

    result = (timeout == 0U) ?
             rt_sem_trytake(&thread->completion_storage) :
             rt_sem_take(&thread->completion_storage,
                         os_to_rt_timeout(timeout));
    if (result != RT_EOK) {
        key = os_enter_critical();
        thread->join_active = false;
        os_exit_critical(key);
        return (timeout == 0U) ? -EBUSY : -ETIMEDOUT;
    }

    os_rt_thread_reap(thread);
    return 0;
}

int os_thread_delete(struct os_thread *thread)
{
    os_critical_key_t key;

    if (!thread || os_is_in_isr()) {
        return !thread ? -EINVAL : -EWOULDBLOCK;
    }

    key = os_enter_critical();
    if (thread->completion_reaped) {
        os_exit_critical(key);
        return 0;
    }
    if (!thread->completion_handle) {
        os_exit_critical(key);
        return -EINVAL;
    }
    if (!thread->completed || !thread->completion_signaled ||
        thread->join_active) {
        os_exit_critical(key);
        return -EBUSY;
    }
    thread->join_active = true;
    os_exit_critical(key);

    os_rt_thread_reap(thread);
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
    os_critical_key_t key;

    if (!thread || !thread->handle) {
        return;
    }
    native_priority = os_to_rt_priority(priority);
    thread->base_priority = priority;
    (void)rt_thread_control((rt_thread_t)thread->handle,
                            RT_THREAD_CTRL_CHANGE_PRIORITY,
                            &native_priority);

    key = os_enter_critical();
    if (thread->wait_q != NULL) {
        os_wait_q_t *wait_q = thread->wait_q;

        sys_dlist_remove(&thread->wait_node);
        os_rt_waitq_insert_locked(wait_q, thread);
    }
    os_exit_critical(key);
}

void os_thread_yield(void)
{
    if (!os_is_in_isr()) {
        (void)rt_thread_yield();
    }
}

#include "internal.h"

#include <string.h>

static void os_task_entry(void *parameter)
{
    struct os_thread *thread = (struct os_thread *)parameter;
    os_critical_key_t key;

    thread->entry(thread->p1, thread->p2, thread->p3);

    key = os_enter_critical();
    thread->completed = true;
    thread->state = OS_THREAD_TERMINATED;
    os_exit_critical(key);
    (void)xSemaphoreGive((SemaphoreHandle_t)thread->completion_handle);
    key = os_enter_critical();
    thread->completion_signaled = true;
    os_exit_critical(key);
    vTaskDelete(NULL);
}

int os_thread_create(struct os_thread *thread,
                     const char *const thread_name,
                     os_thread_stack_t *stack, size_t stack_size,
                     os_thread_entry_t entry,
                     void *p1, void *p2, void *p3,
                     int prio, uint32_t options)
{
    SemaphoreHandle_t completion;
    size_t stack_words;

    (void)options;
    if (!thread || !entry || !thread_name || stack_size == 0U) {
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
    completion = xSemaphoreCreateBinaryStatic(
        (StaticSemaphore_t *)&thread->completion_storage);
    if (completion == NULL) {
        return -ENOMEM;
    }
    thread->completion_handle = completion;
    stack_words = (stack_size + sizeof(StackType_t) - 1U) /
                  sizeof(StackType_t);

    /*
     * Prevent a newly created higher-priority task from running before its
     * OSAL thread-local pointer has been published.
     */
    vTaskSuspendAll();
    if (stack != NULL) {
        thread->handle = xTaskCreateStatic(
            os_task_entry, thread_name, (uint32_t)stack_words, thread,
            (UBaseType_t)prio, (StackType_t *)stack,
            (StaticTask_t *)&thread->tcb);
    } else {
        TaskHandle_t handle = NULL;
        BaseType_t result = xTaskCreate(
            os_task_entry, thread_name, (uint32_t)stack_words, thread,
            (UBaseType_t)prio, &handle);

        if (result == pdPASS) {
            thread->handle = handle;
        }
    }

    if (thread->handle == NULL) {
        (void)xTaskResumeAll();
        vSemaphoreDelete(completion);
        thread->completion_handle = NULL;
        return -ENOMEM;
    }

#if (configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0)
    vTaskSetThreadLocalStoragePointer((TaskHandle_t)thread->handle, 0, thread);
#else
#error "configNUM_THREAD_LOCAL_STORAGE_POINTERS must be > 0"
#endif

    (void)xTaskResumeAll();
    return 0;
}

static void os_thread_reap(struct os_thread *thread)
{
    SemaphoreHandle_t completion;
    os_critical_key_t key = os_enter_critical();

    completion = (SemaphoreHandle_t)thread->completion_handle;
    thread->completion_handle = NULL;
    thread->handle = NULL;
    thread->completion_reaped = true;
    thread->join_active = false;
    os_exit_critical(key);

    if (completion != NULL) {
        vSemaphoreDelete(completion);
    }
}

int os_thread_join(struct os_thread *thread, uint32_t timeout)
{
    SemaphoreHandle_t completion;
    BaseType_t result;
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
    completion = (SemaphoreHandle_t)thread->completion_handle;
    os_exit_critical(key);

    result = xSemaphoreTake(completion,
                            (timeout == OS_WAIT_FOREVER) ?
                            portMAX_DELAY : (TickType_t)timeout);
    if (result != pdTRUE) {
        key = os_enter_critical();
        thread->join_active = false;
        os_exit_critical(key);
        return (timeout == 0U) ? -EBUSY : -ETIMEDOUT;
    }

    os_thread_reap(thread);
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

    os_thread_reap(thread);
    return 0;
}

int os_thread_cancel(struct os_thread *thread)
{
    os_critical_key_t key;

    if (!thread || !thread->handle) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    key = os_enter_critical();
    if (thread->completed) {
        os_exit_critical(key);
        return -EALREADY;
    }
    thread->cancel_requested = true;
    os_exit_critical(key);
    return 0;
}

bool os_thread_cancel_requested(void)
{
    struct os_thread *thread = os_get_current_thread();

    return thread ? thread->cancel_requested : false;
}

int os_thread_test_cancel(void)
{
    return os_thread_cancel_requested() ? -ECANCELED : 0;
}

int os_thread_abort(struct os_thread *thread)
{
    TaskHandle_t handle;
    SemaphoreHandle_t completion;
    struct os_thread *current;
    os_critical_key_t key;

    if (!thread || !thread->handle || !thread->completion_handle) {
        return -EINVAL;
    }
    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }
    current = os_get_current_thread();
    key = os_enter_critical();
    if (thread->completed) {
        os_exit_critical(key);
        return -EALREADY;
    }
    if (!sys_dlist_is_empty(&thread->owned_mutexes)) {
        os_exit_critical(key);
        return -EBUSY;
    }
    if (thread->wait_q != NULL) {
        struct os_thread *owner = thread->waiting_mutex ?
                                  thread->waiting_mutex->owner : NULL;

        sys_dlist_remove(&thread->wait_node);
        thread->wait_q = NULL;
        thread->waiting_mutex = NULL;
        if (owner != NULL) {
            os_mutex_update_pi_chain_locked(owner);
        }
    }
    thread->cancel_requested = true;
    thread->aborted = true;
    thread->completed = true;
    thread->state = OS_THREAD_TERMINATED;
    handle = (TaskHandle_t)thread->handle;
    completion = (SemaphoreHandle_t)thread->completion_handle;
    os_exit_critical(key);

    if (current == thread) {
        (void)xSemaphoreGive(completion);
        key = os_enter_critical();
        thread->completion_signaled = true;
        os_exit_critical(key);
        vTaskDelete(NULL);
        return 0;
    }

    vTaskDelete(handle);
    (void)xSemaphoreGive(completion);
    key = os_enter_critical();
    thread->completion_signaled = true;
    os_exit_critical(key);
    return 0;
}

struct os_thread *os_get_current_thread(void)
{
#if (configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0)
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    return (struct os_thread *)
        pvTaskGetThreadLocalStoragePointer(handle, 0);
#else
    return NULL;
#endif
}

int os_thread_get_priority(struct os_thread *thread)
{
    if (!thread || !thread->handle) {
        return -EINVAL;
    }
    return (int)uxTaskPriorityGet((TaskHandle_t)thread->handle);
}

void os_thread_set_priority(struct os_thread *thread, int priority)
{
    os_critical_key_t key;

    if (!thread || !thread->handle) {
        return;
    }

    key = os_enter_critical();
    thread->base_priority = priority;
    os_mutex_update_pi_chain_locked(thread);
    if ((thread->wait_q != NULL) && (thread->waiting_mutex == NULL)) {
        sys_dlist_remove(&thread->wait_node);
        os_waitq_insert_priority_locked(thread->wait_q, thread);
    }
    os_exit_critical(key);
}

void os_thread_yield(void)
{
    if (!os_is_in_isr()) {
        taskYIELD();
    }
}

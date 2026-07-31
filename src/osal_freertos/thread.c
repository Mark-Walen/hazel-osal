#include "internal.h"

#include <string.h>

static void os_task_entry(void *parameter)
{
    struct os_thread *thread = (struct os_thread *)parameter;

    thread->entry(thread->p1, thread->p2, thread->p3);
    vTaskDelete(NULL);
}

int os_thread_create(struct os_thread *thread,
                     const char *const thread_name,
                     os_thread_stack_t *stack, size_t stack_size,
                     os_thread_entry_t entry,
                     void *p1, void *p2, void *p3,
                     int prio, uint32_t options)
{
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

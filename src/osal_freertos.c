#include <lynx_wireless/kernel.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

/*
 * FreeRTOS port side:
 *   1) If your port already provides xPortIsInsideInterrupt(), use it.
 *   2) Otherwise map it to your ISR nesting counter.
 */
extern BaseType_t xPortIsInsideInterrupt(void);

static void os_task_entry(void *parameter);
static void os_waitq_insert_priority_locked(os_wait_q_t *q,
                                            struct os_thread *thread);
static void os_mutex_update_pi_chain_locked(struct os_thread *thread);

/* ------------------------------------------------------------ */
/* ISR / critical                                                */
/* ------------------------------------------------------------ */

bool os_is_in_isr(void)
{
    return (xPortIsInsideInterrupt() != pdFALSE);
}

os_critical_key_t os_enter_critical(void)
{
    os_critical_key_t key = {
        .value = 0,
        .from_isr = false,
    };

    if (os_is_in_isr()) {
        key.from_isr = true;
        key.value = (uintptr_t)portSET_INTERRUPT_MASK_FROM_ISR();
    } else {
        taskENTER_CRITICAL();
    }

    return key;
}

void os_exit_critical(os_critical_key_t key)
{
    if (key.from_isr) {
        portCLEAR_INTERRUPT_MASK_FROM_ISR((UBaseType_t)key.value);
    } else {
        taskEXIT_CRITICAL();
    }
}

/* ------------------------------------------------------------ */
/* Thread                                                        */
/* ------------------------------------------------------------ */

static void os_task_entry(void *parameter)
{
    struct os_thread *thread = (struct os_thread *)parameter;

    thread->entry(thread->p1, thread->p2, thread->p3);

    /* Thread returned */
    vTaskDelete(NULL);
}

int os_thread_create(struct os_thread *thread,
                     const char * const thread_name,
                     os_thread_stack_t *stack, size_t stack_size,
                     os_thread_entry_t entry,
                     void *p1, void *p2, void *p3,
                     int prio, uint32_t options)
{
    (void)options;

    if (!thread || !entry || !thread_name) {
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

    /*
     * A newly created higher-priority task can run before the create API
     * returns. Keep scheduling suspended until its OSAL TLS pointer is
     * published, otherwise os_get_current_thread() can briefly return NULL.
     */
    vTaskSuspendAll();

    if (stack) {
        /*
         * Static task create:
         * stack_size is in bytes, convert to StackType_t words.
         */
        if (stack_size == 0) {
            return -EINVAL;
        }

        size_t stack_words = (stack_size + sizeof(StackType_t) - 1) / sizeof(StackType_t);

        thread->handle = xTaskCreateStatic(
            os_task_entry,
            thread_name,
            (uint32_t)stack_words,
            thread,
            (UBaseType_t)prio,
            (StackType_t *)stack,
            (StaticTask_t *)&thread->tcb);

    } else {
        /*
         * Dynamic create:
         * stack_size is in bytes, convert to StackType_t words.
         */
        if (stack_size == 0) {
            return -EINVAL;
        }

        size_t stack_words = (stack_size + sizeof(StackType_t) - 1) / sizeof(StackType_t);
        TaskHandle_t handle = NULL;

        BaseType_t ret = xTaskCreate(
            os_task_entry,
            thread_name,
            (uint32_t)stack_words,
            thread,
            (UBaseType_t)prio,
            &handle);

        if (ret != pdPASS) {
            (void)xTaskResumeAll();
            return -ENOMEM;
        }

        thread->handle = handle;
    }

    if (!thread->handle) {
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
    TaskHandle_t h = xTaskGetCurrentTaskHandle();
    return (struct os_thread *)pvTaskGetThreadLocalStoragePointer(h, 0);
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
    if (os_is_in_isr()) {
        return;
    }

    taskYIELD();
}

/* ------------------------------------------------------------ */
/* Wait queue                                                    */
/* ------------------------------------------------------------ */

void os_waitq_init(os_wait_q_t *q)
{
    if (!q) {
        return;
    }

    sys_dlist_init(&q->waitq);
}

static BaseType_t os_waitq_notify_thread_locked(struct os_thread *thread, uint32_t reason)
{
    BaseType_t hpw = pdFALSE;

    if (!thread || !thread->handle) {
        return pdFALSE;
    }

    if (os_is_in_isr()) {
        xTaskNotifyFromISR((TaskHandle_t)thread->handle,
                           reason,
                           eSetValueWithOverwrite,
                           &hpw);
    } else {
        (void)xTaskNotify((TaskHandle_t)thread->handle,
                          reason,
                          eSetValueWithOverwrite);
    }

    return hpw;
}

static void os_waitq_insert_priority_locked(os_wait_q_t *q,
                                            struct os_thread *thread)
{
    sys_dnode_t *node;
    int priority = os_thread_get_priority(thread);

    /*
     * Highest effective priority first. Equal-priority waiters retain FIFO
     * order because insertion skips existing peers.
     */
    SYS_DLIST_FOR_EACH_NODE(&q->waitq, node) {
        struct os_thread *queued;

        queued = CONTAINER_OF(node, struct os_thread, wait_node);
        if (priority > os_thread_get_priority(queued)) {
            break;
        }
    }

    if (node != NULL) {
        sys_dlist_insert(node, &thread->wait_node);
    } else {
        sys_dlist_append(&q->waitq, &thread->wait_node);
    }
}

int os_waitq_block(os_wait_q_t *q, uint32_t timeout)
{
    struct os_thread *thread;
    uint32_t notify = 0;
    BaseType_t ret;

    if (!q) {
        return -EINVAL;
    }

    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    thread = os_get_current_thread();
    if (!thread) {
        return -EINVAL;
    }

    /*
     * Put current thread into waitq first.
     * If wake comes before xTaskNotifyWait(), the notification is pending
     * and xTaskNotifyWait() returns immediately.
     */
    {
        os_critical_key_t key = os_enter_critical();

        thread->wait_q = q;
        thread->wait_result = 0;
        thread->state = OS_THREAD_PENDING;

        os_waitq_insert_priority_locked(q, thread);

        os_exit_critical(key);
    }

    ret = xTaskNotifyWait(0,
                          UINT32_MAX,
                          &notify,
                          (timeout == OS_WAIT_FOREVER) ? portMAX_DELAY : timeout);

    /*
     * If wait_q is still non-NULL, nobody woke us.
     * Remove ourselves from the queue and report timeout.
     */
    {
        os_critical_key_t key = os_enter_critical();

        if (thread->wait_q != NULL) {
            sys_dlist_remove(&thread->wait_node);
            thread->wait_q = NULL;
            thread->state = OS_THREAD_READY;

            os_exit_critical(key);
            return -ETIMEDOUT;
        }

        thread->wait_result = notify;
        thread->state = OS_THREAD_READY;

        os_exit_critical(key);
    }

    return (ret == pdTRUE) ? (int)notify : -ETIMEDOUT;
}

void os_waitq_wake_one(os_wait_q_t *q, uint32_t reason)
{
    os_critical_key_t key;
    struct os_thread *thread;
    BaseType_t hpw = pdFALSE;

    if (!q) {
        return;
    }

    key = os_enter_critical();

    if (sys_dlist_is_empty(&q->waitq)) {
        os_exit_critical(key);
        return;
    }

    thread = CONTAINER_OF(sys_dlist_get(&q->waitq), struct os_thread, wait_node);

    thread->wait_q = NULL;
    thread->wait_result = reason;
    thread->state = OS_THREAD_READY;

    hpw = os_waitq_notify_thread_locked(thread, reason);

    os_exit_critical(key);

    if (os_is_in_isr() && hpw) {
        portYIELD_FROM_ISR(hpw);
    }
}

void os_waitq_wake_all(os_wait_q_t *q, uint32_t reason)
{
    os_critical_key_t key;
    BaseType_t hpw = pdFALSE;

    if (!q) {
        return;
    }

    key = os_enter_critical();

    while (!sys_dlist_is_empty(&q->waitq)) {
        struct os_thread *thread;

        thread = CONTAINER_OF(sys_dlist_get(&q->waitq), struct os_thread, wait_node);

        thread->wait_q = NULL;
        thread->wait_result = reason;
        thread->state = OS_THREAD_READY;

        hpw |= os_waitq_notify_thread_locked(thread, reason);
    }

    os_exit_critical(key);

    if (os_is_in_isr() && hpw) {
        portYIELD_FROM_ISR(hpw);
    }
}

/* ------------------------------------------------------------ */
/* Mutex                                                         */
/* ------------------------------------------------------------ */

void os_mutex_init(struct os_mutex *mutex)
{
    if (!mutex) {
        return;
    }

    memset(mutex, 0, sizeof(*mutex));
    os_waitq_init(&mutex->wait_q);
}

#define OS_PI_MAX_CHAIN_DEPTH 16U

static int os_mutex_compute_priority_locked(struct os_thread *thread)
{
    sys_dnode_t *mutex_node;
    int effective_priority;

    if (!thread || !thread->handle) {
        return -EINVAL;
    }

    effective_priority = thread->base_priority;

    SYS_DLIST_FOR_EACH_NODE(&thread->owned_mutexes, mutex_node) {
        struct os_mutex *owned;
        sys_dnode_t *waiter_node;

        owned = CONTAINER_OF(mutex_node, struct os_mutex, owner_node);
        SYS_DLIST_FOR_EACH_NODE(&owned->wait_q.waitq, waiter_node) {
            struct os_thread *waiter;
            int waiter_priority;

            waiter = CONTAINER_OF(waiter_node, struct os_thread, wait_node);
            waiter_priority = os_thread_get_priority(waiter);
            if (waiter_priority > effective_priority) {
                effective_priority = waiter_priority;
            }
        }
    }

    return effective_priority;
}

static void os_mutex_update_pi_chain_locked(struct os_thread *thread)
{
    struct os_thread *visited[OS_PI_MAX_CHAIN_DEPTH];
    size_t depth = 0;

    while ((thread != NULL) && (depth < OS_PI_MAX_CHAIN_DEPTH)) {
        struct os_mutex *upstream;
        int effective_priority;
        size_t i;

        for (i = 0; i < depth; ++i) {
            if (visited[i] == thread) {
                return;
            }
        }
        visited[depth++] = thread;

        effective_priority = os_mutex_compute_priority_locked(thread);
        if ((effective_priority >= 0) &&
            (os_thread_get_priority(thread) != effective_priority)) {
            vTaskPrioritySet((TaskHandle_t)thread->handle,
                             (UBaseType_t)effective_priority);
        }

        upstream = thread->waiting_mutex;
        if (upstream == NULL) {
            return;
        }

        /*
         * The effective priority may have changed while this thread was
         * blocked. Reinsert it before propagating to the upstream owner.
         */
        sys_dlist_remove(&thread->wait_node);
        os_waitq_insert_priority_locked(&upstream->wait_q, thread);
        thread = upstream->owner;
    }
}

static bool os_mutex_would_deadlock_locked(struct os_thread *thread,
                                           struct os_mutex *mutex)
{
    struct os_thread *owner = mutex->owner;
    size_t depth;

    for (depth = 0; owner != NULL && depth < OS_PI_MAX_CHAIN_DEPTH; ++depth) {
        if (owner == thread) {
            return true;
        }
        if (owner->waiting_mutex == NULL) {
            return false;
        }
        owner = owner->waiting_mutex->owner;
    }

    /* Refuse an unbounded or already cyclic dependency chain. */
    return owner != NULL;
}

int os_mutex_lock(struct os_mutex *mutex, uint32_t timeout)
{
    struct os_thread *curr;
    uint32_t start;

    if (!mutex) {
        return -EINVAL;
    }

    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    curr = os_get_current_thread();
    if (!curr) {
        return -EINVAL;
    }

    start = os_tick_get();

    for (;;) {
        uint32_t remaining = OS_WAIT_FOREVER;
        uint32_t notify = 0;
        BaseType_t notified;
        os_critical_key_t key = os_enter_critical();

        if (mutex->owner == NULL) {
            mutex->owner = curr;
            mutex->lock_count = 1;
            sys_dlist_append(&curr->owned_mutexes, &mutex->owner_node);
            os_exit_critical(key);
            return 0;
        }

        if (mutex->owner == curr) {
            mutex->lock_count++;
            os_exit_critical(key);
            return 0;
        }

        if (timeout == 0U) {
            os_exit_critical(key);
            return -EBUSY;
        }

        if (os_mutex_would_deadlock_locked(curr, mutex)) {
            os_exit_critical(key);
            return -EDEADLK;
        }

        if (timeout != OS_WAIT_FOREVER) {
            uint32_t elapsed = os_tick_get() - start;

            if (elapsed >= timeout) {
                os_exit_critical(key);
                return -ETIMEDOUT;
            }
            remaining = timeout - elapsed;
        }

        /*
         * Queueing and inspecting owner are one critical operation, preventing
         * an unlock between the ownership check and waiter publication.
         */
        curr->wait_q = &mutex->wait_q;
        curr->waiting_mutex = mutex;
        curr->wait_result = 0;
        curr->state = OS_THREAD_PENDING;
        os_waitq_insert_priority_locked(&mutex->wait_q, curr);
        os_mutex_update_pi_chain_locked(mutex->owner);
        os_exit_critical(key);

        notified = xTaskNotifyWait(0, UINT32_MAX, &notify,
                                   (remaining == OS_WAIT_FOREVER) ?
                                   portMAX_DELAY : (TickType_t)remaining);

        key = os_enter_critical();
        if (curr->wait_q != NULL) {
            struct os_thread *owner = mutex->owner;

            sys_dlist_remove(&curr->wait_node);
            curr->wait_q = NULL;
            curr->waiting_mutex = NULL;
            curr->state = OS_THREAD_READY;
            os_mutex_update_pi_chain_locked(owner);
            os_exit_critical(key);
            return (notified == pdTRUE) ? -EAGAIN : -ETIMEDOUT;
        }

        curr->state = OS_THREAD_READY;
        curr->waiting_mutex = NULL;
        os_exit_critical(key);
        /* The unlock path selected us; retry and claim the unowned mutex. */
    }
}

int os_mutex_trylock(struct os_mutex *mutex)
{
    return os_mutex_lock(mutex, 0U);
}

int os_mutex_unlock(struct os_mutex *mutex)
{
    struct os_thread *curr;
    os_critical_key_t key;

    if (!mutex) {
        return -EINVAL;
    }

    if (os_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    curr = os_get_current_thread();
    if (!curr) {
        return -EINVAL;
    }

    key = os_enter_critical();

    if (mutex->owner != curr) {
        os_exit_critical(key);
        return -EPERM;
    }

    if (mutex->lock_count == 0) {
        os_exit_critical(key);
        return -EINVAL;
    }

    if (--mutex->lock_count > 0) {
        os_exit_critical(key);
        return 0;
    }

    sys_dlist_remove(&mutex->owner_node);
    mutex->owner = NULL;

    /*
     * Wake one waiter while still protected.
     */
    if (!sys_dlist_is_empty(&mutex->wait_q.waitq)) {
        struct os_thread *thread;
        BaseType_t hpw;

        thread = CONTAINER_OF(sys_dlist_get(&mutex->wait_q.waitq),
                              struct os_thread,
                              wait_node);

        thread->wait_q = NULL;
        thread->waiting_mutex = NULL;
        thread->wait_result = 0;
        thread->state = OS_THREAD_READY;

        hpw = os_waitq_notify_thread_locked(thread, 0);
        (void)hpw;
    }

    /*
     * Recalculate across every mutex still owned by the thread. This preserves
     * inheritance from unrelated waiters when mutexes are nested.
     */
    os_mutex_update_pi_chain_locked(curr);

    os_exit_critical(key);
    return 0;
}

/* ------------------------------------------------------------ */
/* Memory / time                                                 */
/* ------------------------------------------------------------ */

void *os_malloc(size_t size)
{
    if (size == 0U) {
        return NULL;
    }

    if (os_is_in_isr()) {
        return NULL;
    }

    return pvPortMalloc(size);
}

void os_free(void *ptr)
{
    if (!ptr) {
        return;
    }

    if (os_is_in_isr()) {
        return;
    }

    vPortFree(ptr);
}

uint32_t os_tick_get(void)
{
    if (os_is_in_isr()) {
        return (uint32_t)xTaskGetTickCountFromISR();
    }

    return (uint32_t)xTaskGetTickCount();
}

void os_delay(uint32_t ticks)
{
    if (os_is_in_isr()) {
        return;
    }

    vTaskDelay((TickType_t)ticks);
}

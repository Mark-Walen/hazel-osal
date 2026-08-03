/**
 * @file kernel.h
 * @brief Lynx Wireless Kernel Abstraction Layer API.
 *
 * This module provides a generic API for threading, mutual exclusion,
 * timing, and memory management, abstracting the underlying RTOS.
 */
#ifndef LYNX_INCLUDE_KERNEL_H_
#define LYNX_INCLUDE_KERNEL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#if defined(__has_include)
#if __has_include(<errno.h>)
#include <errno.h>
#endif
#else
#include <errno.h>
#endif

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EAGAIN
#define EAGAIN 11
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif
#ifndef EBUSY
#define EBUSY 16
#endif
#ifndef EPERM
#define EPERM 1
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif
#ifndef EDEADLK
#define EDEADLK 35
#endif
#ifndef ENOSPC
#define ENOSPC 28
#endif
#ifndef ENODEV
#define ENODEV 19
#endif
#ifndef ECANCELED
#define ECANCELED 125
#endif
#ifndef EALREADY
#define EALREADY 114
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

#include <lynx_wireless/osal/osal.h>
#include <lynx_wireless/sys/util.h>
#include <lynx_wireless/sys/dlist.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Timeout value indicating the operation should wait indefinitely.
 */
#ifndef OS_WAIT_FOREVER
#define OS_WAIT_FOREVER UINT32_MAX
#endif

#ifdef CONFIG_POLL
#define Z_POLL_EVENT_OBJ_INIT(obj) \
        .poll_events = SYS_DLIST_STATIC_INIT(&obj.poll_events),
#define Z_DECL_POLL_EVENT sys_dlist_t poll_events;
#else
#define Z_POLL_EVENT_OBJ_INIT(obj)
#define Z_DECL_POLL_EVENT
#endif

struct os_thread;
struct os_mutex;
struct os_sem;
struct os_work;
struct os_work_delayable;
struct os_work_queue;

/**
 * @brief Wait queue structure.
 *
 * Used to queue threads waiting for a specific event or resource.
 */
typedef struct {
    sys_dlist_t waitq; /**< Linked list of waiting threads. */
} os_wait_q_t;

/**
 * @brief Thread entry point function signature.
 *
 * @param p1 First argument passed to the thread.
 * @param p2 Second argument passed to the thread.
 * @param p3 Third argument passed to the thread.
 */
typedef void (*os_thread_entry_t)(void *p1, void *p2, void *p3);

#ifndef os_thread_stack_t
#define os_thread_stack_t void *
#endif

#ifndef os_thread_tcb_t
#define os_thread_tcb_t void *
#endif

#ifndef os_sem_storage_t
#define os_sem_storage_t void *
#endif

#ifndef os_thread_wait_storage_t
#define os_thread_wait_storage_t void *
#endif

#ifndef os_mutex_storage_t
#define os_mutex_storage_t void *
#endif

#ifndef os_thread_completion_storage_t
#define os_thread_completion_storage_t void *
#endif

#ifndef os_event_storage_t
#define os_event_storage_t void *
#endif

/**
 * @brief Key structure for critical section management.
 *
 * Stores the state required to restore interrupts upon exiting a critical section.
 */
typedef struct {
    uintptr_t value;   /**< Architecture-specific interrupt mask value. */
    bool from_isr;     /**< Flag indicating if the critical section was entered from an ISR. */
} os_critical_key_t;

/**
 * @brief Thread execution states.
 */
enum os_thread_state {
    OS_THREAD_READY = 0, /**< Thread is ready to run. */
    OS_THREAD_PENDING,   /**< Thread is blocked waiting for an event. */
    OS_THREAD_TERMINATED, /**< Entry function returned. */
};

/**
 * @brief Structure representing a thread.
 */
struct os_thread {
    void *handle;           /**< RTOS-specific thread handle. */

    os_thread_stack_t *stack; /**< Pointer to the stack memory (for static allocation). */
    os_thread_tcb_t tcb;      /**< Thread Control Block storage (for static allocation). */
    size_t stack_size;        /**< Size of the stack in bytes. */

    os_thread_entry_t entry;  /**< Thread entry function. */
    void *p1;                 /**< First argument to entry function. */
    void *p2;                 /**< Second argument to entry function. */
    void *p3;                 /**< Third argument to entry function. */

    sys_dnode_t wait_node;    /**< Node for linking into wait queues. */
    os_wait_q_t *wait_q;      /**< Pointer to the wait queue the thread is currently blocked on. */
    struct os_mutex *waiting_mutex; /**< Mutex currently awaited for transitive PI. */
    uint32_t wait_result;     /**< Result value received when woken up. */
    enum os_thread_state state; /**< Current state of the thread. */

    int base_priority;          /**< Priority requested by the application. */
    sys_dlist_t owned_mutexes;  /**< Mutexes currently owned by this thread. */
    os_thread_wait_storage_t wait_storage; /**< Backend wait notification. */
    void *completion_handle;    /**< Backend completion-event handle. */
    os_thread_completion_storage_t completion_storage;
    bool completed;             /**< Entry function has returned. */
    bool completion_signaled;   /**< Completion event has been published. */
    bool completion_reaped;     /**< Native OSAL resources were reclaimed. */
    bool join_active;           /**< A thread is currently joining this thread. */
    bool cancel_requested;      /**< Cooperative cancellation was requested. */
    bool aborted;               /**< Thread was terminated by os_thread_abort(). */
};

/**
 * @brief Structure representing a mutex.
 */
struct os_mutex {
    os_wait_q_t wait_q;      /**< Queue of threads waiting for the mutex. */
    struct os_thread *owner; /**< Pointer to the thread currently holding the mutex. */
    uint32_t lock_count;     /**< Recursion count (number of locks by owner). */
    sys_dnode_t owner_node;  /**< Link in the owner's mutex list. */
    void *native_handle;     /**< Native mutex handle for native-IPC backends. */
    os_mutex_storage_t storage; /**< Native static mutex storage. */
    bool initialized;        /**< Mutex was successfully initialized. */
};

/**
 * @brief Counting semaphore.
 *
 * The API and lifetime are RTOS-independent. os_sem_storage_t is selected by
 * the backend adapter so the object can embed FreeRTOS, RT-Thread, or Zephyr
 * native storage without dynamic allocation.
 */
struct os_sem {
    void *handle;                 /**< Backend-native semaphore handle. */
    os_sem_storage_t storage;     /**< Backend-native static storage. */
    uint32_t limit;               /**< Maximum count. */
};

/** Fixed-size, caller-buffered message queue. */
struct os_msgq {
    uint8_t *buffer;
    size_t message_size;
    uint32_t capacity;
    uint32_t read_index;
    uint32_t write_index;
    uint32_t count;
    struct os_sem items;
    struct os_sem spaces;
    bool initialized;
};

/** Node embedded in an object placed on an os_fifo. */
struct os_fifo_node {
    sys_dnode_t node;
    bool queued;
};

/** Intrusive, zero-copy FIFO. */
struct os_fifo {
    sys_dlist_t list;
    struct os_sem items;
    uint32_t count;
    bool initialized;
};

/** Fixed-size block allocator backed by caller-owned memory. */
struct os_mem_slab {
    uint8_t *buffer;
    void *free_list;
    size_t block_size;
    uint32_t num_blocks;
    uint32_t free_blocks;
    struct os_sem available;
    bool initialized;
};

/** Portable event bits; the high byte is reserved by some backends. */
#ifndef OS_EVENT_BITS_MASK
#define OS_EVENT_BITS_MASK UINT32_C(0x00ffffff)
#endif

struct os_event {
    void *handle;
    os_event_storage_t storage;
    bool initialized;
};

/** Single pending notification carrying an integer result. */
struct os_signal {
    struct os_sem notification;
    int result;
    bool raised;
    bool initialized;
};

typedef void (*os_work_handler_t)(struct os_work *work);

struct os_work {
    struct os_fifo_node node;
    os_work_handler_t handler;
    struct os_work_queue *queue;
    struct os_sem sync;
    uint32_t flags;
    uint32_t submission_id;
    uint32_t running_id;
    uint32_t completion_id;
    uint32_t magic;
    bool sync_initialized;
};

enum os_work_state {
    OS_WORK_RUNNING = 1U << 0,
    OS_WORK_CANCELING = 1U << 1,
    OS_WORK_QUEUED = 1U << 2,
    OS_WORK_DELAYED = 1U << 3,
};

struct os_work_delayable {
    struct os_work work;
    sys_dnode_t delay_node;
    struct os_work_queue *queue;
    uint32_t deadline;
};

struct os_work_sync {
    uintptr_t reserved;
};

struct os_work_queue_config {
    const char *name;
    bool no_yield;
};

struct os_work_queue {
    struct os_fifo fifo;
    sys_dlist_t delayed;
    struct os_fifo_node stop_node;
    struct os_thread thread;
    struct os_work *current;
    bool started;
    bool stopping;
    bool draining;
    bool plugged;
    bool no_yield;
};

/**
 * @defgroup Kernel_Lifecycle Kernel lifecycle
 * @brief Initialize the OS abstraction layer.
 * @{
 */

/**
 * @brief Initialize backend-owned OSAL state.
 *
 * This function is idempotent and must be called from thread context before
 * other OSAL APIs. The platform remains responsible for initializing the
 * selected RTOS and starting its scheduler.
 *
 * @return 0 on success, or -EWOULDBLOCK when called from interrupt context.
 */
int os_kernel_init(void);
/** @} */

/**
 * @defgroup Kernel_WaitQ Wait Queues
 * @brief Functions for managing wait queues.
 * @{
 */

/**
 * @brief Initialize a wait queue.
 *
 * @param q Pointer to the wait queue structure.
 */
void os_waitq_init(os_wait_q_t *q);

/**
 * @brief Block the current thread on a wait queue.
 *
 * @param q Pointer to the wait queue.
 * @param timeout Timeout in system ticks. Use OS_WAIT_FOREVER to wait indefinitely.
 * @return 0 on success (woken up), negative error code on timeout or failure.
 */
int os_waitq_block(os_wait_q_t *q, uint32_t timeout);

/**
 * @brief Wake one thread from the wait queue.
 *
 * @param q Pointer to the wait queue.
 * @param reason Reason code to pass to the woken thread.
 */
void os_waitq_wake_one(os_wait_q_t *q, uint32_t reason);

/**
 * @brief Wake all threads from the wait queue.
 *
 * @param q Pointer to the wait queue.
 * @param reason Reason code to pass to the woken threads.
 */
void os_waitq_wake_all(os_wait_q_t *q, uint32_t reason);
/** @} */

/**
 * @defgroup Kernel_Thread Threads
 * @brief Functions for managing threads.
 * @{
 */

/**
 * @brief Create a new thread.
 *
 * @param thread Pointer to the thread structure to initialize.
 * @param thread_name Name of the thread (string).
 * @param stack Pointer to stack memory (for static creation) or NULL (for dynamic creation).
 * @param stack_size Size of the stack in bytes.
 * @param entry Thread entry function.
 * @param p1 First argument to the thread entry.
 * @param p2 Second argument to the thread entry.
 * @param p3 Third argument to the thread entry.
 * @param prio Thread priority.
 * @param options Thread options (unused in this port).
 * @return 0 on success, negative error code on failure.
 */
int os_thread_create(struct os_thread *thread,
                     const char * const thread_name,
                     os_thread_stack_t *stack, size_t stack_size,
                     os_thread_entry_t entry,
                     void *p1, void *p2, void *p3,
                     int prio, uint32_t options);

/**
 * @brief Wait for a thread to return from its entry function and reap it.
 *
 * Only one joiner may wait at a time. Joining self is rejected. A successful
 * join may be repeated and returns immediately.
 *
 * @param thread Thread to join.
 * @param timeout Timeout in ticks, zero for non-blocking, or OS_WAIT_FOREVER.
 * @return 0 on success, -EBUSY for a zero-timeout miss or another joiner,
 * -ETIMEDOUT on timeout, -EDEADLK for self-join, or another error.
 */
int os_thread_join(struct os_thread *thread, uint32_t timeout);

/**
 * @brief Reap a thread that has already returned.
 *
 * This function never force-terminates a running thread. It returns -EBUSY
 * until the entry function has returned and its completion event is visible.
 *
 * @return 0 on success, -EBUSY while running, or another negative error.
 */
int os_thread_delete(struct os_thread *thread);

/** Request cooperative cancellation; the target remains responsible to exit. */
int os_thread_cancel(struct os_thread *thread);

/** Return true when cancellation was requested for the current thread. */
bool os_thread_cancel_requested(void);

/** Return -ECANCELED at an explicit cooperative cancellation point. */
int os_thread_test_cancel(void);

/**
 * Immediately terminate a thread and make it joinable.
 *
 * FreeRTOS rejects abort while the target owns an OSAL mutex. RT-Thread
 * releases native mutexes as part of its thread-close path.
 */
int os_thread_abort(struct os_thread *thread);

/**
 * @brief Get the current thread handle.
 *
 * @return Pointer to the current thread structure.
 */
struct os_thread *os_get_current_thread(void);

/**
 * @brief Get the priority of a thread.
 *
 * @param thread Pointer to the thread structure.
 * @return Priority value, or negative error code on failure.
 */
int os_thread_get_priority(struct os_thread *thread);

/**
 * @brief Set the priority of a thread.
 *
 * @param thread Pointer to the thread structure.
 * @param priority New priority value.
 */
void os_thread_set_priority(struct os_thread *thread, int priority);

/**
 * @brief Yield the CPU to other ready threads.
 */
void os_thread_yield(void);
/** @} */

/**
 * @defgroup Kernel_Mutex Mutex
 * @brief Functions for mutual exclusion.
 * @{
 */

/**
 * @brief Initialize a mutex.
 *
 * @param mutex Pointer to the mutex structure.
 */
void os_mutex_init(struct os_mutex *mutex);

/**
 * @brief Deinitialize an unlocked mutex with no waiters.
 *
 * @return 0 on success, -EBUSY if owned or awaited, or -EINVAL.
 */
int os_mutex_deinit(struct os_mutex *mutex);

/**
 * @brief Lock a mutex.
 *
 * If the mutex is already held by another thread, the current thread will block.
 * Supports recursive locking if the same thread attempts to lock it again.
 *
 * @param mutex Pointer to the mutex structure.
 * @param timeout Timeout in system ticks.
 * @return 0 on success, negative error code on failure.
 */
int os_mutex_lock(struct os_mutex *mutex, uint32_t timeout);

/**
 * @brief Try to lock a mutex without waiting.
 *
 * @param mutex Pointer to the mutex structure.
 * @return 0 on success, -EBUSY if owned by another thread, or another
 * negative error code on failure.
 */
int os_mutex_trylock(struct os_mutex *mutex);

/**
 * @brief Unlock a mutex.
 *
 * @param mutex Pointer to the mutex structure.
 * @return 0 on success, negative error code on failure.
 */
int os_mutex_unlock(struct os_mutex *mutex);
/** @} */

/**
 * @defgroup Kernel_Semaphore Counting semaphores
 * @brief Portable counting semaphore functions.
 * @{
 */

/**
 * @brief Initialize a counting semaphore.
 *
 * @param sem Semaphore object.
 * @param initial_count Initial available token count.
 * @param limit Maximum token count; must be greater than zero.
 * @return 0 on success or -EINVAL for invalid arguments.
 */
int os_sem_init(struct os_sem *sem, uint32_t initial_count, uint32_t limit);

/**
 * @brief Deinitialize a semaphore.
 *
 * The caller must ensure no thread is waiting on the semaphore.
 *
 * @return 0 on success, -EBUSY if waiters can be detected, or -EINVAL.
 */
int os_sem_deinit(struct os_sem *sem);

/**
 * @brief Take one semaphore token.
 *
 * In interrupt context only a zero timeout is permitted.
 *
 * @param sem Semaphore object.
 * @param timeout Timeout in ticks, zero for non-blocking, or OS_WAIT_FOREVER.
 * @return 0 on success, -EBUSY for a failed non-blocking take,
 * -ETIMEDOUT on timeout, or another negative error code.
 */
int os_sem_take(struct os_sem *sem, uint32_t timeout);

/**
 * @brief Try to take one semaphore token without waiting.
 *
 * @return 0 on success, -EBUSY when no token is available, or -EINVAL.
 */
int os_sem_trytake(struct os_sem *sem);

/**
 * @brief Give one semaphore token.
 *
 * Giving an already-full semaphore is a successful no-op. This saturating
 * behavior provides the same semantics across supported RTOS backends.
 *
 * @param sem Semaphore object.
 */
void os_sem_give(struct os_sem *sem);

/**
 * @brief Return the current available token count.
 *
 * @return Current count, or zero for an invalid semaphore.
 */
uint32_t os_sem_count_get(struct os_sem *sem);
/** @} */

/** Initialize/deinitialize a fixed-size message queue. */
int os_msgq_init(struct os_msgq *msgq, void *buffer, size_t message_size,
                 uint32_t capacity);
int os_msgq_deinit(struct os_msgq *msgq);
int os_msgq_put(struct os_msgq *msgq, const void *message, uint32_t timeout);
int os_msgq_get(struct os_msgq *msgq, void *message, uint32_t timeout);
uint32_t os_msgq_count_get(struct os_msgq *msgq);

/** Initialize/deinitialize and access an intrusive FIFO. */
int os_fifo_init(struct os_fifo *fifo);
int os_fifo_deinit(struct os_fifo *fifo);
void os_fifo_node_init(struct os_fifo_node *node);
int os_fifo_put(struct os_fifo *fifo, struct os_fifo_node *node);
int os_fifo_get(struct os_fifo *fifo, struct os_fifo_node **node,
                uint32_t timeout);

/** Initialize/deinitialize and allocate from a fixed-size memory slab. */
int os_mem_slab_init(struct os_mem_slab *slab, void *buffer,
                     size_t block_size, uint32_t num_blocks);
int os_mem_slab_deinit(struct os_mem_slab *slab);
int os_mem_slab_alloc(struct os_mem_slab *slab, void **memory,
                      uint32_t timeout);
int os_mem_slab_free(struct os_mem_slab *slab, void *memory);
uint32_t os_mem_slab_num_free_get(struct os_mem_slab *slab);

/** Event-group operations. Wait returns matching bits through received. */
int os_event_init(struct os_event *event);
int os_event_deinit(struct os_event *event);
int os_event_set(struct os_event *event, uint32_t bits);
int os_event_clear(struct os_event *event, uint32_t bits);
int os_event_wait(struct os_event *event, uint32_t requested, bool wait_all,
                  bool clear, uint32_t timeout, uint32_t *received);

/** Raise, wait for, and reset a result-carrying signal. */
int os_signal_init(struct os_signal *signal);
int os_signal_deinit(struct os_signal *signal);
int os_signal_raise(struct os_signal *signal, int result);
int os_signal_wait(struct os_signal *signal, uint32_t timeout, int *result);
int os_signal_reset(struct os_signal *signal);

/** Cooperative work queue backed by an OSAL worker thread. */
void os_work_init(struct os_work *work, os_work_handler_t handler);
int os_work_queue_start(struct os_work_queue *queue,
                        os_thread_stack_t *stack, size_t stack_size,
                        int priority,
                        const struct os_work_queue_config *config);
int os_work_submit_to_queue(struct os_work_queue *queue,
                            struct os_work *work);
int os_work_submit(struct os_work_queue *queue, struct os_work *work);
uint32_t os_work_busy_get(const struct os_work *work);
bool os_work_is_pending(const struct os_work *work);
int os_work_cancel(struct os_work *work);
bool os_work_cancel_sync(struct os_work *work, struct os_work_sync *sync);
bool os_work_flush(struct os_work *work, struct os_work_sync *sync);
void os_work_init_delayable(struct os_work_delayable *dwork,
                            os_work_handler_t handler);
struct os_work_delayable *os_work_delayable_from_work(struct os_work *work);
uint32_t os_work_delayable_busy_get(const struct os_work_delayable *dwork);
bool os_work_delayable_is_pending(const struct os_work_delayable *dwork);
uint32_t os_work_delayable_expires_get(const struct os_work_delayable *dwork);
uint32_t os_work_delayable_remaining_get(
    const struct os_work_delayable *dwork);
int os_work_schedule_for_queue(struct os_work_queue *queue,
                               struct os_work_delayable *dwork,
                               uint32_t delay);
int os_work_reschedule_for_queue(struct os_work_queue *queue,
                                 struct os_work_delayable *dwork,
                                 uint32_t delay);
int os_work_schedule(struct os_work_queue *queue,
                     struct os_work_delayable *dwork, uint32_t delay);
int os_work_reschedule(struct os_work_queue *queue,
                       struct os_work_delayable *dwork, uint32_t delay);
int os_work_cancel_delayable(struct os_work_delayable *dwork);
bool os_work_cancel_delayable_sync(struct os_work_delayable *dwork,
                                   struct os_work_sync *sync);
bool os_work_flush_delayable(struct os_work_delayable *dwork,
                             struct os_work_sync *sync);
int os_work_queue_drain(struct os_work_queue *queue, bool plug);
int os_work_queue_unplug(struct os_work_queue *queue);
int os_work_queue_stop(struct os_work_queue *queue, uint32_t timeout);
struct os_thread *os_work_queue_thread_get(struct os_work_queue *queue);

/**
 * @brief Check if execution context is an Interrupt Service Routine.
 *
 * @return true if in ISR, false otherwise.
 */
bool os_is_in_isr(void);

/**
 * @brief Enter a critical section.
 *
 * Interrupts are masked to prevent context switching.
 *
 * @return A key structure required to exit the critical section.
 */
os_critical_key_t os_enter_critical(void);

/**
 * @brief Exit a critical section.
 *
 * Restores the interrupt state saved in the key.
 *
 * @param key The key returned by os_enter_critical.
 */
void os_exit_critical(os_critical_key_t key);

/**
 * @brief Allocate memory from the heap.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory, or NULL on failure.
 */
void *os_malloc(size_t size);

/**
 * @brief Free allocated memory.
 *
 * @param ptr Pointer to memory to free.
 */
void os_free(void *ptr);

/**
 * @brief Get the current system tick count.
 *
 * @return Current tick count.
 */
uint32_t os_tick_get(void);

/**
 * @brief Get monotonic uptime in milliseconds.
 *
 * @return Milliseconds elapsed since the RTOS tick source started.
 */
uint64_t os_uptime_get(void);

/**
 * @brief Convert milliseconds to ticks, rounding up.
 *
 * Values that do not fit saturate at OS_WAIT_FOREVER - 1 because
 * OS_WAIT_FOREVER is reserved as a timeout sentinel.
 */
uint32_t os_ms_to_ticks_ceil(uint64_t milliseconds);

/**
 * @brief Convert ticks to milliseconds, rounding down.
 */
uint64_t os_ticks_to_ms(uint64_t ticks);

/**
 * @brief Delay execution for a specified number of ticks.
 *
 * @param ticks Number of ticks to sleep.
 */
void os_delay(uint32_t ticks);

#ifdef __cplusplus
}
#endif

#endif /* LYNX_INCLUDE_KERNEL_H_ */

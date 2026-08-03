/**
 * @file kernel.h
 * @brief Hazel Wireless Kernel Abstraction Layer API.
 *
 * This module provides a generic API for threading, mutual exclusion,
 * timing, memory management, IPC, events, atomic services, and deferred work
 * while abstracting the underlying RTOS.
 */
#ifndef HAZEL_INCLUDE_KERNEL_H_
#define HAZEL_INCLUDE_KERNEL_H_

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

#include <hazel_wireless/osal/osal.h>
#include <hazel_wireless/sys/util.h>
#include <hazel_wireless/sys/dlist.h>
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
    os_thread_completion_storage_t completion_storage; /**< Backend completion-event storage. */
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

/** @brief Fixed-size, caller-buffered message queue. */
struct os_msgq {
    uint8_t *buffer;            /**< Caller-owned ring storage. */
    size_t message_size;        /**< Size of each message in bytes. */
    uint32_t capacity;          /**< Number of messages the ring can hold. */
    uint32_t read_index;        /**< Index of the next message to read. */
    uint32_t write_index;       /**< Index at which the next message is put. */
    uint32_t count;             /**< Number of messages currently queued. */
    struct os_sem items;        /**< Count of messages available to readers. */
    struct os_sem spaces;       /**< Count of slots available to writers. */
    bool initialized;           /**< True after successful initialization. */
};

/** @brief Node embedded in an object placed on an os_fifo. */
struct os_fifo_node {
    sys_dnode_t node;           /**< Intrusive list linkage. */
    bool queued;                /**< True while owned by a FIFO. */
};

/** @brief Intrusive, zero-copy FIFO. */
struct os_fifo {
    sys_dlist_t list;           /**< Ordered list of queued nodes. */
    struct os_sem items;        /**< Count of nodes available to readers. */
    uint32_t count;             /**< Number of queued nodes. */
    bool initialized;           /**< True after successful initialization. */
};

/** @brief Fixed-size block allocator backed by caller-owned memory. */
struct os_mem_slab {
    uint8_t *buffer;            /**< Start of caller-owned block storage. */
    void *free_list;            /**< Head of the internal free-block list. */
    size_t block_size;          /**< Size of one block in bytes. */
    uint32_t num_blocks;        /**< Total number of blocks. */
    uint32_t free_blocks;       /**< Number of blocks currently free. */
    struct os_sem available;    /**< Count of blocks available to allocators. */
    bool initialized;           /**< True after successful initialization. */
};

/** Portable event bits; the high byte is reserved by some backends. */
#ifndef OS_EVENT_BITS_MASK
#define OS_EVENT_BITS_MASK UINT32_C(0x00ffffff)
#endif

/** @brief Portable event group containing up to OS_EVENT_BITS_MASK bits. */
struct os_event {
    void *handle;               /**< Backend-native event object handle. */
    os_event_storage_t storage; /**< Embedded backend event storage. */
    bool initialized;           /**< True after successful initialization. */
};

/** @brief Single pending notification carrying an integer result. */
struct os_signal {
    struct os_sem notification; /**< Binary notification for a waiter. */
    int result;                 /**< Result associated with the pending signal. */
    bool raised;                /**< True while one signal is pending. */
    bool initialized;           /**< True after successful initialization. */
};

/**
 * @brief Work item handler invoked by a work-queue thread.
 * @param work Work item being executed.
 */
typedef void (*os_work_handler_t)(struct os_work *work);

/** @brief Immediate work item and its synchronization state. */
struct os_work {
    struct os_fifo_node node;   /**< Queue linkage for an immediate submission. */
    os_work_handler_t handler;  /**< Function invoked for each submission. */
    struct os_work_queue *queue; /**< Most recently selected work queue. */
    struct os_sem sync;         /**< Completion notification for synchronous APIs. */
    uint32_t flags;             /**< Bitwise combination of os_work_state values. */
    uint32_t submission_id;     /**< Monotonic identifier of the latest submission. */
    uint32_t running_id;        /**< Submission identifier currently executing. */
    uint32_t completion_id;     /**< Most recently completed submission identifier. */
    uint32_t magic;             /**< Internal initialization signature. */
    bool sync_initialized;      /**< True when the completion semaphore is valid. */
};

/** @brief Work item state flags returned by os_work_busy_get(). */
enum os_work_state {
    OS_WORK_RUNNING = 1U << 0,  /**< Handler is executing. */
    OS_WORK_CANCELING = 1U << 1, /**< Cancellation awaits handler completion. */
    OS_WORK_QUEUED = 1U << 2,   /**< Work is queued for execution. */
    OS_WORK_DELAYED = 1U << 3,  /**< Delayable work awaits its deadline. */
};

/** @brief Work item with a tick-based deferred submission deadline. */
struct os_work_delayable {
    struct os_work work;         /**< Embedded immediate work item. */
    sys_dnode_t delay_node;      /**< Link in a queue's delayed-item list. */
    struct os_work_queue *queue; /**< Queue selected for delayed submission. */
    uint32_t deadline;           /**< Absolute 32-bit tick deadline. */
};

/** @brief Opaque caller-owned state for synchronous work operations. */
struct os_work_sync {
    uintptr_t reserved;          /**< Reserved for backend-independent ABI use. */
};

/** @brief Optional work-queue startup configuration. */
struct os_work_queue_config {
    const char *name;             /**< Worker name, or NULL for a default name. */
    bool no_yield;                /**< Suppress the yield after each handler. */
};

/** @brief Work queue, worker thread, and immediate/delayed item state. */
struct os_work_queue {
    struct os_fifo fifo;          /**< Immediate work submissions. */
    sys_dlist_t delayed;          /**< Delayable work awaiting deadlines. */
    struct os_fifo_node stop_node; /**< Internal worker termination marker. */
    struct os_thread thread;      /**< Thread executing work handlers. */
    struct os_work *current;      /**< Work item whose handler is running. */
    bool started;                 /**< True while the worker exists. */
    bool stopping;                /**< True after stop has been requested. */
    bool draining;                /**< True while external submissions are blocked. */
    bool plugged;                 /**< True while submissions remain blocked. */
    bool no_yield;                /**< Copy of the startup no-yield option. */
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
 * @param thread Thread to reap.
 * @return 0 on success, -EBUSY while running, or another negative error.
 */
int os_thread_delete(struct os_thread *thread);

/**
 * @brief Request cooperative cancellation of a running thread.
 *
 * The request only sets a flag. The target must call os_thread_test_cancel()
 * or os_thread_cancel_requested() and return from its entry function.
 *
 * @param thread Thread to request cancellation from.
 * @return 0 on success, -EALREADY if complete, -EWOULDBLOCK from an ISR, or
 * -EINVAL for an invalid thread.
 */
int os_thread_cancel(struct os_thread *thread);

/**
 * @brief Test the current thread's cooperative cancellation flag.
 * @return true if cancellation was requested, otherwise false.
 */
bool os_thread_cancel_requested(void);

/**
 * @brief Cooperative cancellation point for the current thread.
 * @return -ECANCELED when cancellation was requested, otherwise 0.
 */
int os_thread_test_cancel(void);

/**
 * @brief Immediately terminate a thread and make it joinable.
 *
 * FreeRTOS rejects abort while the target owns an OSAL mutex. RT-Thread
 * releases native mutexes as part of its thread-close path.
 *
 * @param thread Thread to terminate.
 * @return 0 on success, -EBUSY when termination would abandon an OSAL mutex,
 * -EDEADLK when a backend cannot abort the caller, or another negative error.
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
 * @param mutex Mutex to deinitialize.
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
 * @param sem Semaphore object.
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
 * @param sem Semaphore object.
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
 * @param sem Semaphore object.
 * @return Current count, or zero for an invalid semaphore.
 */
uint32_t os_sem_count_get(struct os_sem *sem);
/** @} */

/**
 * @defgroup Kernel_MessageQueue Message queues
 * @brief Fixed-size copied-message queues backed by caller storage.
 * @{
 */

/**
 * @brief Initialize a message queue.
 * @param msgq Queue object to initialize.
 * @param buffer Storage for @p capacity messages.
 * @param message_size Size of one message in bytes.
 * @param capacity Maximum number of queued messages.
 * @return 0 on success, -EINVAL for invalid arguments, or -ENOMEM when a
 * backend synchronization object cannot be initialized.
 */
int os_msgq_init(struct os_msgq *msgq, void *buffer, size_t message_size,
                 uint32_t capacity);

/**
 * @brief Deinitialize a message queue.
 *
 * The caller must ensure the queue is empty and no thread is waiting.
 * @param msgq Queue object.
 * @return 0 on success or a negative synchronization-object error.
 */
int os_msgq_deinit(struct os_msgq *msgq);

/**
 * @brief Copy a message into a queue.
 * @param msgq Queue object.
 * @param message Address of exactly the configured message size.
 * @param timeout Timeout in ticks, zero for non-blocking, or OS_WAIT_FOREVER.
 * @return 0 on success, -EBUSY if a non-blocking put finds no space,
 * -ETIMEDOUT on timeout, or another negative error.
 */
int os_msgq_put(struct os_msgq *msgq, const void *message, uint32_t timeout);

/**
 * @brief Copy the oldest message out of a queue.
 * @param msgq Queue object.
 * @param message Destination with at least the configured message size.
 * @param timeout Timeout in ticks, zero for non-blocking, or OS_WAIT_FOREVER.
 * @return 0 on success, -EBUSY if a non-blocking get finds no message,
 * -ETIMEDOUT on timeout, or another negative error.
 */
int os_msgq_get(struct os_msgq *msgq, void *message, uint32_t timeout);

/**
 * @brief Get a snapshot of the number of queued messages.
 * @param msgq Queue object.
 * @return Current message count, or zero for an invalid queue.
 */
uint32_t os_msgq_count_get(struct os_msgq *msgq);
/** @} */

/**
 * @defgroup Kernel_FIFO Intrusive FIFO
 * @brief Zero-copy FIFO using nodes embedded in caller objects.
 * @{
 */

/**
 * @brief Initialize an empty FIFO.
 * @param fifo FIFO object.
 * @return 0 on success or a negative initialization error.
 */
int os_fifo_init(struct os_fifo *fifo);

/**
 * @brief Deinitialize an empty FIFO with no waiters.
 * @param fifo FIFO object.
 * @return 0 on success, -EBUSY if nodes remain, or another negative error.
 */
int os_fifo_deinit(struct os_fifo *fifo);

/**
 * @brief Initialize a node before its first FIFO insertion.
 * @param node Node embedded in the caller's object.
 */
void os_fifo_node_init(struct os_fifo_node *node);

/**
 * @brief Append a node to a FIFO.
 * @param fifo FIFO object.
 * @param node Initialized node not currently owned by a FIFO.
 * @return 0 on success, -EBUSY if already queued, or another negative error.
 */
int os_fifo_put(struct os_fifo *fifo, struct os_fifo_node *node);

/**
 * @brief Remove the oldest FIFO node.
 * @param fifo FIFO object.
 * @param node Receives the removed node on success.
 * @param timeout Timeout in ticks, zero for non-blocking, or OS_WAIT_FOREVER.
 * @return 0 on success, -EBUSY for an empty non-blocking get, -ETIMEDOUT on
 * timeout, or another negative error.
 */
int os_fifo_get(struct os_fifo *fifo, struct os_fifo_node **node,
                uint32_t timeout);
/** @} */

/**
 * @defgroup Kernel_MemorySlab Memory slabs
 * @brief Fixed-size allocation from aligned caller-owned storage.
 * @{
 */

/**
 * @brief Initialize a memory slab.
 * @param slab Slab object.
 * @param buffer Aligned storage for all blocks.
 * @param block_size Size of each block; large enough to store a pointer.
 * @param num_blocks Number of blocks in @p buffer.
 * @return 0 on success or -EINVAL when arguments or alignment are invalid.
 */
int os_mem_slab_init(struct os_mem_slab *slab, void *buffer,
                     size_t block_size, uint32_t num_blocks);

/**
 * @brief Deinitialize a slab after every block has been returned.
 * @param slab Slab object.
 * @return 0 on success, -EBUSY while blocks are allocated, or another error.
 */
int os_mem_slab_deinit(struct os_mem_slab *slab);

/**
 * @brief Allocate one block.
 * @param slab Slab object.
 * @param memory Receives the allocated block address.
 * @param timeout Timeout in ticks, zero for non-blocking, or OS_WAIT_FOREVER.
 * @return 0 on success, -EBUSY for a non-blocking miss, -ETIMEDOUT on
 * timeout, or another negative error.
 */
int os_mem_slab_alloc(struct os_mem_slab *slab, void **memory,
                      uint32_t timeout);

/**
 * @brief Return a block to its slab.
 * @param slab Slab object.
 * @param memory Block previously returned by os_mem_slab_alloc().
 * @return 0 on success or -EINVAL for a foreign, misaligned, or free block.
 */
int os_mem_slab_free(struct os_mem_slab *slab, void *memory);

/**
 * @brief Get a snapshot of the number of free blocks.
 * @param slab Slab object.
 * @return Number of free blocks, or zero for an invalid slab.
 */
uint32_t os_mem_slab_num_free_get(struct os_mem_slab *slab);
/** @} */

/**
 * @defgroup Kernel_Event Event groups
 * @brief Portable bit-mask notification and waiting.
 * @{
 */

/**
 * @brief Initialize an event group with no bits set.
 * @param event Event object.
 * @return 0 on success or a negative initialization error.
 */
int os_event_init(struct os_event *event);

/**
 * @brief Deinitialize an event group with no waiters.
 * @param event Event object.
 * @return 0 on success, -EBUSY if waiters are detected, or another error.
 */
int os_event_deinit(struct os_event *event);

/**
 * @brief Atomically set event bits.
 * @param event Event object.
 * @param bits Bits within OS_EVENT_BITS_MASK to set.
 * @return 0 on success or -EINVAL for invalid arguments.
 */
int os_event_set(struct os_event *event, uint32_t bits);

/**
 * @brief Atomically clear event bits.
 * @param event Event object.
 * @param bits Bits within OS_EVENT_BITS_MASK to clear.
 * @return 0 on success or -EINVAL for invalid arguments.
 */
int os_event_clear(struct os_event *event, uint32_t bits);

/**
 * @brief Wait until any or all requested event bits are set.
 * @param event Event object.
 * @param requested Non-zero mask of bits to wait for.
 * @param wait_all If true require all requested bits; otherwise require any.
 * @param clear If true clear the matching requested bits before returning.
 * @param timeout Timeout in ticks, zero for non-blocking, or OS_WAIT_FOREVER.
 * @param received Receives the matching bits on success.
 * @return 0 on success, -EBUSY for an unsatisfied non-blocking wait,
 * -ETIMEDOUT on timeout, or another negative error.
 * @note Event operations are thread-context-only on the portable contract.
 */
int os_event_wait(struct os_event *event, uint32_t requested, bool wait_all,
                  bool clear, uint32_t timeout, uint32_t *received);
/** @} */

/**
 * @defgroup Kernel_Signal Signals
 * @brief Single-pending notification carrying an integer result.
 * @{
 */

/**
 * @brief Initialize a signal in the not-raised state.
 * @param signal Signal object.
 * @return 0 on success or a negative initialization error.
 */
int os_signal_init(struct os_signal *signal);

/**
 * @brief Deinitialize a signal with no waiter.
 * @param signal Signal object.
 * @return 0 on success, -EBUSY if a waiter is detected, or another error.
 */
int os_signal_deinit(struct os_signal *signal);

/**
 * @brief Raise a signal with an integer result.
 * @param signal Signal object.
 * @param result Result delivered to the waiter.
 * @return 0 on success or -EBUSY if a signal is already pending.
 */
int os_signal_raise(struct os_signal *signal, int result);

/**
 * @brief Wait for and consume a pending signal.
 * @param signal Signal object.
 * @param timeout Timeout in ticks, zero for non-blocking, or OS_WAIT_FOREVER.
 * @param result Receives the raised integer result.
 * @return 0 on success, -EBUSY for a non-blocking miss, -ETIMEDOUT on
 * timeout, or another negative error.
 */
int os_signal_wait(struct os_signal *signal, uint32_t timeout, int *result);

/**
 * @brief Discard a pending signal without consuming its result.
 * @param signal Signal object.
 * @return 0 on success or -EINVAL for an invalid signal.
 */
int os_signal_reset(struct os_signal *signal);
/** @} */

/**
 * @defgroup Kernel_WorkQueue Work queues
 * @brief Zephyr-style immediate and delayable deferred work.
 * @{
 */

/**
 * @brief Initialize an immediate work item.
 * @param work Work object, which must be idle before reinitialization.
 * @param handler Non-NULL function invoked by the work-queue thread.
 */
void os_work_init(struct os_work *work, os_work_handler_t handler);

/**
 * @brief Start a work queue and its worker thread.
 * @param queue Queue object.
 * @param stack Static stack storage, or NULL for backend dynamic allocation.
 * @param stack_size Stack size in bytes.
 * @param priority Worker priority in the portable OSAL priority domain.
 * @param config Optional queue configuration, or NULL for defaults.
 * @return 0 on success, -EALREADY if started, or another negative error.
 */
int os_work_queue_start(struct os_work_queue *queue,
                        os_thread_stack_t *stack, size_t stack_size,
                        int priority,
                        const struct os_work_queue_config *config);

/**
 * @brief Submit an immediate work item to a queue.
 * @param queue Target queue; NULL reuses the most recent target queue.
 * @param work Initialized work item.
 * @return 0 if already queued, 1 if newly queued, 2 if running and queued
 * again, or a negative error.
 */
int os_work_submit_to_queue(struct os_work_queue *queue,
                            struct os_work *work);

/**
 * @brief Submit work using the OSAL explicit-queue convenience API.
 * @param queue Target work queue.
 * @param work Initialized work item.
 * @return As documented by os_work_submit_to_queue().
 */
int os_work_submit(struct os_work_queue *queue, struct os_work *work);

/**
 * @brief Get a live snapshot of work state flags.
 * @param work Work item.
 * @return Bitwise combination of os_work_state values, or zero if idle.
 */
uint32_t os_work_busy_get(const struct os_work *work);

/**
 * @brief Test whether a work item is queued, running, canceling, or delayed.
 * @param work Work item.
 * @return true if any busy flag is set, otherwise false.
 */
bool os_work_is_pending(const struct os_work *work);

/**
 * @brief Cancel a queued immediate work submission without waiting.
 * @param work Work item.
 * @return Remaining busy flags, zero when cancellation is complete, or a
 * negative validation error.
 */
int os_work_cancel(struct os_work *work);

/**
 * @brief Cancel immediate work and wait until it becomes idle.
 * @param work Work item.
 * @param sync Caller-owned synchronization object valid until return.
 * @return true if work was pending, otherwise false.
 * @note Must not be called by the handler currently executing @p work.
 */
bool os_work_cancel_sync(struct os_work *work, struct os_work_sync *sync);

/**
 * @brief Wait for the last-submitted immediate work instance to finish.
 * @param work Work item.
 * @param sync Caller-owned synchronization object valid until return.
 * @return true if the call waited, otherwise false.
 * @note Must not be called by the handler currently executing @p work.
 */
bool os_work_flush(struct os_work *work, struct os_work_sync *sync);

/**
 * @brief Initialize a delayable work item.
 * @param dwork Delayable work object.
 * @param handler Non-NULL handler receiving the embedded os_work pointer.
 */
void os_work_init_delayable(struct os_work_delayable *dwork,
                            os_work_handler_t handler);

/**
 * @brief Recover a delayable work object from its embedded work item.
 * @param work Embedded work pointer passed to a handler.
 * @return Containing delayable work, or NULL when @p work is NULL.
 */
struct os_work_delayable *os_work_delayable_from_work(struct os_work *work);

/**
 * @brief Get a live snapshot of delayable work state flags.
 * @param dwork Delayable work item.
 * @return Bitwise combination of os_work_state values, or zero if idle.
 */
uint32_t os_work_delayable_busy_get(const struct os_work_delayable *dwork);

/**
 * @brief Test whether delayable work has any busy state flag.
 * @param dwork Delayable work item.
 * @return true if pending, otherwise false.
 */
bool os_work_delayable_is_pending(const struct os_work_delayable *dwork);

/**
 * @brief Get the absolute tick deadline of scheduled delayable work.
 * @param dwork Delayable work item.
 * @return Deadline when delayed, otherwise the current tick count.
 */
uint32_t os_work_delayable_expires_get(const struct os_work_delayable *dwork);

/**
 * @brief Get ticks remaining before delayed submission.
 * @param dwork Delayable work item.
 * @return Remaining ticks, or zero when not delayed or already due.
 */
uint32_t os_work_delayable_remaining_get(
    const struct os_work_delayable *dwork);

/**
 * @brief Schedule idle work after a delay without changing an existing schedule.
 * @param queue Target queue.
 * @param dwork Delayable work item.
 * @param delay Delay in ticks, from zero through INT32_MAX.
 * @return 0 if already scheduled or queued, 1 if scheduled, 2 for a zero-delay
 * resubmission of running work, or a negative error.
 */
int os_work_schedule_for_queue(struct os_work_queue *queue,
                               struct os_work_delayable *dwork,
                               uint32_t delay);

/**
 * @brief Replace a pending deadline or schedule work in any state.
 * @param queue Target queue.
 * @param dwork Delayable work item.
 * @param delay New delay in ticks, from zero through INT32_MAX.
 * @return 1 if scheduled, the immediate-submit result for zero delay, or a
 * negative error.
 */
int os_work_reschedule_for_queue(struct os_work_queue *queue,
                                 struct os_work_delayable *dwork,
                                 uint32_t delay);

/**
 * @brief Schedule idle work after a delay.
 * @param queue Target queue.
 * @param dwork Delayable work item.
 * @param delay Delay in ticks, from zero through INT32_MAX.
 * @return 0 if already scheduled or queued, 1 if scheduled, 2 for a
 * zero-delay resubmission of running work, or a negative error.
 */
int os_work_schedule(struct os_work_queue *queue,
                     struct os_work_delayable *dwork, uint32_t delay);

/**
 * @brief Replace a pending deadline or schedule work in any state.
 * @param queue Target queue.
 * @param dwork Delayable work item.
 * @param delay New delay in ticks, from zero through INT32_MAX.
 * @return 1 if scheduled, the immediate-submit result for zero delay, or a
 * negative error.
 */
int os_work_reschedule(struct os_work_queue *queue,
                       struct os_work_delayable *dwork, uint32_t delay);

/**
 * @brief Cancel scheduled or queued delayable work without waiting.
 * @param dwork Delayable work item.
 * @return Remaining busy flags, zero when idle, or a negative error.
 */
int os_work_cancel_delayable(struct os_work_delayable *dwork);

/**
 * @brief Cancel delayable work and wait until it becomes idle.
 * @param dwork Delayable work item.
 * @param sync Caller-owned synchronization object valid until return.
 * @return true if work was pending, otherwise false.
 */
bool os_work_cancel_delayable_sync(struct os_work_delayable *dwork,
                                   struct os_work_sync *sync);

/**
 * @brief Submit delayed work immediately and wait for completion.
 * @param dwork Delayable work item.
 * @param sync Caller-owned synchronization object valid until return.
 * @return true if delayed or if the call waited, otherwise false.
 */
bool os_work_flush_delayable(struct os_work_delayable *dwork,
                             struct os_work_sync *sync);

/**
 * @brief Wait until immediate work drains, optionally retaining a submission plug.
 * @param queue Work queue.
 * @param plug Keep external submissions blocked after draining when true.
 * @return 1 if the call waited, 0 if already drained, or a negative error.
 * @note Delayable work that has not reached its deadline is not part of drain.
 */
int os_work_queue_drain(struct os_work_queue *queue, bool plug);

/**
 * @brief Release a retained work-queue submission plug.
 * @param queue Work queue.
 * @return 0 on success or -EALREADY if the queue was not plugged.
 */
int os_work_queue_unplug(struct os_work_queue *queue);

/**
 * @brief Stop a queue that was first drained with @p plug set true.
 * @param queue Work queue.
 * @param timeout Maximum ticks to wait for worker termination.
 * @return 0 on success, -EBUSY if not drained and plugged, -ETIMEDOUT on
 * timeout, -EALREADY if stopped, or another negative error.
 */
int os_work_queue_stop(struct os_work_queue *queue, uint32_t timeout);

/**
 * @brief Access the thread executing work for a queue.
 * @param queue Work queue.
 * @return Address of the embedded worker thread, or NULL for a NULL queue.
 */
struct os_thread *os_work_queue_thread_get(struct os_work_queue *queue);
/** @} */

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
 *
 * @param milliseconds Duration in milliseconds.
 * @return Equivalent tick count rounded toward positive infinity.
 */
uint32_t os_ms_to_ticks_ceil(uint64_t milliseconds);

/**
 * @brief Convert ticks to milliseconds, rounding down.
 * @param ticks Duration in system ticks.
 * @return Equivalent milliseconds rounded toward zero, saturating on overflow.
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

#endif /* HAZEL_INCLUDE_KERNEL_H_ */

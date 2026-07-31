#include <lynx_wireless/kernel.h>

#include "FreeRTOS.h"
#include "task.h"
#include "platform.h"

#include <stdint.h>

extern void freertos_risc_v_trap_handler(void);

static struct os_thread controller_thread;
static struct os_thread wait_thread;
static struct os_thread mutex_thread;
static struct os_thread ordered_low_thread;
static struct os_thread ordered_high_thread;
static struct os_thread pi_owner_thread;
static struct os_thread pi_medium_thread;
static struct os_thread pi_high_thread;
static struct os_thread sem_thread;
static struct os_thread chain_bottom_thread;
static struct os_thread chain_middle_thread;
static struct os_thread chain_high_thread;
static os_thread_stack_t wait_stack[384];
static os_thread_stack_t mutex_stack[384];
static os_thread_stack_t ordered_low_stack[384];
static os_thread_stack_t ordered_high_stack[384];
static os_thread_stack_t pi_owner_stack[384];
static os_thread_stack_t pi_medium_stack[384];
static os_thread_stack_t pi_high_stack[384];
static os_thread_stack_t sem_stack[384];
static os_thread_stack_t chain_bottom_stack[384];
static os_thread_stack_t chain_middle_stack[384];
static os_thread_stack_t chain_high_stack[384];
static os_wait_q_t test_waitq;
static os_wait_q_t ordered_waitq;
static struct os_mutex test_mutex;
static struct os_mutex pi_mutex;
static struct os_sem test_sem;
static struct os_mutex chain_m1;
static struct os_mutex chain_m2;

static volatile int wait_result;
static volatile int mutex_locked;
static volatile int mutex_done;
static volatile int ordered_ready;
static volatile int ordered_count;
static volatile int ordered_result[2];
static volatile int pi_owner_locked;
static volatile int pi_waiters_ready;
static volatile int pi_owner_boosted_priority;
static volatile int pi_owner_recomputed_priority;
static volatile int pi_owner_restored_priority;
static volatile int pi_owner_done;
static volatile int pi_medium_result;
static volatile int pi_high_result;
static volatile int sem_result;
static volatile int chain_bottom_locked;
static volatile int chain_middle_locked;
static volatile int chain_release_bottom;
static volatile int chain_bottom_done;
static volatile int chain_middle_result;
static volatile int chain_high_result;
static unsigned int checks;

#define TEST_CHECK(condition) \
    do { \
        ++checks; \
        if (!(condition)) { \
            platform_puts("FAIL: " #condition "\n"); \
            platform_exit(0); \
        } \
    } while (0)

void osal_test_assert(const char *file, uint32_t line)
{
    (void)file;
    (void)line;
    platform_puts("FAIL: FreeRTOS assertion\n");
    platform_exit(0);
}

static void wait_task(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;
    wait_result = os_waitq_block(&test_waitq, 50);
}

static void mutex_task(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;

    if (os_mutex_lock(&test_mutex, OS_WAIT_FOREVER) != 0) {
        mutex_done = -1;
        return;
    }
    mutex_locked = 1;
    os_delay(5);
    if (os_mutex_unlock(&test_mutex) != 0) {
        mutex_done = -1;
        return;
    }
    mutex_done = 1;
}

static void ordered_wait_task(void *p1, void *p2, void *p3)
{
    int id = (int)(uintptr_t)p1;

    (void)p2;
    (void)p3;
    ++ordered_ready;
    if (os_waitq_block(&ordered_waitq, 30) == 7) {
        int index = ordered_count++;
        ordered_result[index] = id;
    }
}

static void pi_owner_task(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;

    if (os_mutex_lock(&pi_mutex, OS_WAIT_FOREVER) != 0) {
        pi_owner_done = -1;
        return;
    }
    pi_owner_locked = 1;

    while (pi_waiters_ready < 2) {
        os_delay(1);
    }

    pi_owner_boosted_priority = os_thread_get_priority(&pi_owner_thread);
    os_delay(5);
    pi_owner_recomputed_priority = os_thread_get_priority(&pi_owner_thread);

    if (os_mutex_unlock(&pi_mutex) != 0) {
        pi_owner_done = -1;
        return;
    }
    pi_owner_restored_priority = os_thread_get_priority(&pi_owner_thread);
    pi_owner_done = 1;
}

static void pi_waiter_task(void *p1, void *p2, void *p3)
{
    volatile int *result = (volatile int *)p1;
    uint32_t timeout = (uint32_t)(uintptr_t)p2;
    int ret;

    (void)p3;
    ++pi_waiters_ready;
    ret = os_mutex_lock(&pi_mutex, timeout);
    *result = ret;
    if (ret == 0) {
        (void)os_mutex_unlock(&pi_mutex);
    }
}

static void sem_waiter_task(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;
    sem_result = os_sem_take(&test_sem, 20);
}

static void chain_bottom_task(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;

    if (os_mutex_lock(&chain_m2, OS_WAIT_FOREVER) != 0) {
        chain_bottom_done = -1;
        return;
    }
    chain_bottom_locked = 1;
    while (!chain_release_bottom) {
        os_delay(1);
    }
    if (os_mutex_unlock(&chain_m2) != 0) {
        chain_bottom_done = -1;
        return;
    }
    chain_bottom_done = 1;
}

static void chain_middle_task(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;

    if (os_mutex_lock(&chain_m1, OS_WAIT_FOREVER) != 0) {
        chain_middle_result = -1;
        return;
    }
    chain_middle_locked = 1;
    chain_middle_result = os_mutex_lock(&chain_m2, 30);
    if (chain_middle_result == 0) {
        (void)os_mutex_unlock(&chain_m2);
        (void)os_mutex_unlock(&chain_m1);
    }
}

static void chain_high_task(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;

    chain_high_result = os_mutex_lock(&chain_m1, 30);
    if (chain_high_result == 0) {
        (void)os_mutex_unlock(&chain_m1);
    }
}

static void controller_task(void *p1, void *p2, void *p3)
{
    uint32_t start;
    void *allocation;

    (void)p1;
    (void)p2;
    (void)p3;

    platform_puts("OSAL QEMU RV32 / FreeRTOS\n");

    TEST_CHECK(!os_is_in_isr());
    TEST_CHECK(os_get_current_thread() == &controller_thread);
    TEST_CHECK(os_thread_get_priority(&controller_thread) == 3);

    os_thread_set_priority(&controller_thread, 4);
    TEST_CHECK(os_thread_get_priority(&controller_thread) == 4);
    os_thread_set_priority(&controller_thread, 3);

    allocation = os_malloc(73);
    TEST_CHECK(allocation != 0);
    os_free(allocation);
    TEST_CHECK(os_malloc(0) == 0);

    start = os_tick_get();
    os_delay(3);
    TEST_CHECK((uint32_t)(os_tick_get() - start) >= 3U);

    os_waitq_init(&test_waitq);
    wait_result = -999;
    TEST_CHECK(os_thread_create(&wait_thread, "wait",
                                wait_stack, sizeof(wait_stack),
                                wait_task, 0, 0, 0, 2, 0) == 0);
    os_delay(2);
    os_waitq_wake_one(&test_waitq, 42);
    while (wait_result == -999) {
        os_delay(1);
    }
    TEST_CHECK(wait_result == 42);

    os_waitq_init(&ordered_waitq);
    ordered_ready = 0;
    ordered_count = 0;
    ordered_result[0] = 0;
    ordered_result[1] = 0;
    TEST_CHECK(os_thread_create(&ordered_low_thread, "order-low",
                                ordered_low_stack, sizeof(ordered_low_stack),
                                ordered_wait_task, (void *)(uintptr_t)2,
                                0, 0, 2, 0) == 0);
    TEST_CHECK(os_thread_create(&ordered_high_thread, "order-high",
                                ordered_high_stack, sizeof(ordered_high_stack),
                                ordered_wait_task, (void *)(uintptr_t)4,
                                0, 0, 4, 0) == 0);
    while (ordered_ready < 2) {
        os_delay(1);
    }
    os_thread_set_priority(&ordered_low_thread, 5);
    os_waitq_wake_one(&ordered_waitq, 7);
    while (ordered_count < 1) {
        os_delay(1);
    }
    TEST_CHECK(ordered_result[0] == 2);
    os_waitq_wake_one(&ordered_waitq, 7);
    while (ordered_count < 2) {
        os_delay(1);
    }
    TEST_CHECK(ordered_result[1] == 4);

    os_mutex_init(&test_mutex);
    TEST_CHECK(os_mutex_trylock(&test_mutex) == 0);
    TEST_CHECK(os_mutex_trylock(&test_mutex) == 0);
    TEST_CHECK(os_mutex_unlock(&test_mutex) == 0);
    TEST_CHECK(os_mutex_unlock(&test_mutex) == 0);
    TEST_CHECK(os_mutex_unlock(&test_mutex) == -EPERM);

    mutex_locked = 0;
    mutex_done = 0;
    TEST_CHECK(os_thread_create(&mutex_thread, "mutex",
                                mutex_stack, sizeof(mutex_stack),
                                mutex_task, 0, 0, 0, 4, 0) == 0);
    while (!mutex_locked && mutex_done == 0) {
        os_delay(1);
    }
    TEST_CHECK(mutex_done != -1);
    TEST_CHECK(os_mutex_trylock(&test_mutex) == -EBUSY);
    TEST_CHECK(os_mutex_lock(&test_mutex, 20) == 0);
    TEST_CHECK(mutex_done == 1);
    TEST_CHECK(os_mutex_unlock(&test_mutex) == 0);

    os_mutex_init(&pi_mutex);
    pi_owner_locked = 0;
    pi_waiters_ready = 0;
    pi_owner_done = 0;
    pi_medium_result = -999;
    pi_high_result = -999;
    TEST_CHECK(os_thread_create(&pi_owner_thread, "pi-owner",
                                pi_owner_stack, sizeof(pi_owner_stack),
                                pi_owner_task, 0, 0, 0, 1, 0) == 0);
    while (!pi_owner_locked) {
        os_delay(1);
    }
    TEST_CHECK(os_thread_create(&pi_medium_thread, "pi-medium",
                                pi_medium_stack, sizeof(pi_medium_stack),
                                pi_waiter_task, (void *)&pi_medium_result,
                                (void *)(uintptr_t)20, 0, 4, 0) == 0);
    TEST_CHECK(os_thread_create(&pi_high_thread, "pi-high",
                                pi_high_stack, sizeof(pi_high_stack),
                                pi_waiter_task, (void *)&pi_high_result,
                                (void *)(uintptr_t)3, 0, 5, 0) == 0);
    while (pi_owner_done == 0 || pi_medium_result == -999 ||
           pi_high_result == -999) {
        os_delay(1);
    }
    TEST_CHECK(pi_owner_done == 1);
    TEST_CHECK(pi_owner_boosted_priority == 5);
    TEST_CHECK(pi_high_result == -ETIMEDOUT);
    TEST_CHECK(pi_owner_recomputed_priority == 4);
    TEST_CHECK(pi_medium_result == 0);
    TEST_CHECK(pi_owner_restored_priority == 1);

    TEST_CHECK(os_sem_init(&test_sem, 2, 1) == -EINVAL);
    TEST_CHECK(os_sem_init(&test_sem, 1, 2) == 0);
    TEST_CHECK(os_sem_count_get(&test_sem) == 1);
    TEST_CHECK(os_sem_trytake(&test_sem) == 0);
    TEST_CHECK(os_sem_count_get(&test_sem) == 0);
    TEST_CHECK(os_sem_trytake(&test_sem) == -EBUSY);
    os_sem_give(&test_sem);
    os_sem_give(&test_sem);
    os_sem_give(&test_sem);
    TEST_CHECK(os_sem_count_get(&test_sem) == 2);
    TEST_CHECK(os_sem_take(&test_sem, 0) == 0);
    TEST_CHECK(os_sem_take(&test_sem, 0) == 0);
    TEST_CHECK(os_sem_take(&test_sem, 2) == -ETIMEDOUT);

    sem_result = -999;
    TEST_CHECK(os_thread_create(&sem_thread, "sem",
                                sem_stack, sizeof(sem_stack),
                                sem_waiter_task, 0, 0, 0, 4, 0) == 0);
    os_delay(2);
    TEST_CHECK(sem_result == -999);
    os_sem_give(&test_sem);
    while (sem_result == -999) {
        os_delay(1);
    }
    TEST_CHECK(sem_result == 0);
    TEST_CHECK(os_sem_count_get(&test_sem) == 0);

    os_mutex_init(&chain_m1);
    os_mutex_init(&chain_m2);
    chain_bottom_locked = 0;
    chain_middle_locked = 0;
    chain_release_bottom = 0;
    chain_bottom_done = 0;
    chain_middle_result = -999;
    chain_high_result = -999;

    TEST_CHECK(os_thread_create(&chain_bottom_thread, "pi-bottom",
                                chain_bottom_stack, sizeof(chain_bottom_stack),
                                chain_bottom_task, 0, 0, 0, 1, 0) == 0);
    while (!chain_bottom_locked) {
        os_delay(1);
    }
    TEST_CHECK(os_thread_create(&chain_middle_thread, "pi-middle",
                                chain_middle_stack, sizeof(chain_middle_stack),
                                chain_middle_task, 0, 0, 0, 2, 0) == 0);
    while (!chain_middle_locked) {
        os_delay(1);
    }
    TEST_CHECK(os_thread_create(&chain_high_thread, "pi-chain-hi",
                                chain_high_stack, sizeof(chain_high_stack),
                                chain_high_task, 0, 0, 0, 5, 0) == 0);
    os_delay(1);

    TEST_CHECK(os_thread_get_priority(&chain_middle_thread) == 5);
    TEST_CHECK(os_thread_get_priority(&chain_bottom_thread) == 5);

    chain_release_bottom = 1;
    while ((chain_bottom_done == 0) || (chain_middle_result == -999) ||
           (chain_high_result == -999)) {
        os_delay(1);
    }
    TEST_CHECK(chain_bottom_done == 1);
    TEST_CHECK(chain_middle_result == 0);
    TEST_CHECK(chain_high_result == 0);

    (void)checks;
    platform_puts("PASS: OSAL tests\n");
    platform_exit(1);
}

int main(void)
{
    __asm__ volatile("csrw mtvec, %0" : : "r"(freertos_risc_v_trap_handler));

    if (os_thread_create(&controller_thread, "controller",
                         0, 1024,
                         controller_task, 0, 0, 0, 3, 0) != 0) {
        platform_puts("FAIL: controller creation\n");
        platform_exit(0);
    }

    vTaskStartScheduler();
    platform_puts("FAIL: scheduler returned\n");
    platform_exit(0);
}

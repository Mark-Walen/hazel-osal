/* Zephyr counting semaphore backend. */
#include <hazel_wireless/kernel.h>

#include <zephyr/kernel.h>

int os_sem_init(struct os_sem *sem, uint32_t initial_count, uint32_t limit)
{
    int result;

    if (!sem || (limit == 0U) || (initial_count > limit)) {
        return -EINVAL;
    }

    result = k_sem_init(&sem->storage, initial_count, limit);
    if (result != 0) {
        return -EINVAL;
    }

    sem->handle = &sem->storage;
    sem->limit = limit;
    return 0;
}

int os_sem_deinit(struct os_sem *sem)
{
    if (!sem || !sem->handle) {
        return -EINVAL;
    }
    if (k_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    sem->handle = NULL;
    sem->limit = 0U;
    return 0;
}

int os_sem_take(struct os_sem *sem, uint32_t timeout)
{
    k_timeout_t wait;
    int result;

    if (!sem || !sem->handle) {
        return -EINVAL;
    }
    if (k_is_in_isr() && (timeout != 0U)) {
        return -EWOULDBLOCK;
    }

    wait = (timeout == OS_WAIT_FOREVER) ? K_FOREVER :
           ((timeout == 0U) ? K_NO_WAIT : K_TICKS(timeout));
    result = k_sem_take(&sem->storage, wait);
    if (result == 0) {
        return 0;
    }
    return (timeout == 0U) ? -EBUSY : -ETIMEDOUT;
}

int os_sem_trytake(struct os_sem *sem)
{
    return os_sem_take(sem, 0U);
}

void os_sem_give(struct os_sem *sem)
{
    if (sem && sem->handle) {
        k_sem_give(&sem->storage);
    }
}

uint32_t os_sem_count_get(struct os_sem *sem)
{
    if (!sem || !sem->handle) {
        return 0U;
    }
    return k_sem_count_get(&sem->storage);
}

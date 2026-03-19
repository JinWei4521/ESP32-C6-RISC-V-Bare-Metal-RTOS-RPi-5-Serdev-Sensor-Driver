#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define MAX_TASKS 3
#define STACK_SIZE 2048

// Define task states
typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_SUSPENDED,
    TASK_BLOCKED
} task_state_t;

// TCB
typedef struct {
    uint32_t *sp;        // Must be the first member for assembly access
    task_state_t state;
    uint32_t id;
    void (*entry)(void);
    uint32_t wake_tick;
} tcb_t;

typedef struct {
    volatile uint8_t locked;      // 0: unlocked, 1: locked
    volatile int owner_task_id;   // Task ID (-1 if no owner)
} os_mutex_t;

void os_task_create(int task_id, uint8_t *stack, void (*task_entry)(void));
uint32_t** os_task_switch(void);
void os_start_scheduler(void);
void os_delay(uint32_t ticks);
void os_yield(void);
extern volatile uint32_t os_tick_count; // This is exposed for timer.c
void os_enter_critical(void);
void os_exit_critical(void);

// Mutex API
void os_mutex_init(os_mutex_t *mutex);
void os_mutex_take(os_mutex_t *mutex);
void os_mutex_give(os_mutex_t *mutex);

#endif // TASK_H
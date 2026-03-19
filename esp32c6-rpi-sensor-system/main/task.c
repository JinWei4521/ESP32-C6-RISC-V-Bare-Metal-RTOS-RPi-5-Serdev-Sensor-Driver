#include "task.h"

#define SYS_YIELD           0
#define SYS_ENTER_CRITICAL  1
#define SYS_EXIT_CRITICAL   2

extern void vector_table(void); 

tcb_t tasks[MAX_TASKS];
volatile int current_task_idx = 0;
volatile uint32_t os_tick_count = 0;

void os_task_create(int task_id, uint8_t *stack, void (*task_entry)(void)) {
    uint32_t *top = (uint32_t *)(stack + STACK_SIZE);
    top = (uint32_t *)((uint32_t)top & ~0xF); 
    top -= 36; 
    for(int i = 0; i < 36; i++) top[i] = 0;

    uint32_t gp, tp;
    __asm__ volatile ("mv %0, gp" : "=r"(gp));
    __asm__ volatile ("mv %0, tp" : "=r"(tp));
    top[3] = gp;
    top[4] = tp;
    top[32] = (uint32_t)task_entry; 
    top[33] = 0x0080;               

    tasks[task_id].sp = top;
    tasks[task_id].state = TASK_READY;
    tasks[task_id].id = task_id;
    tasks[task_id].entry = task_entry;
}

uint32_t** os_task_switch(void) {
    /* 1. Wake up suspended tasks if their sleep time is reached */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SUSPENDED && os_tick_count >= tasks[i].wake_tick) {
            tasks[i].state = TASK_READY;
        }
    }

    /* 2. Set current running task back to READY (if not blocked/suspended) */
    if (tasks[current_task_idx].state == TASK_RUNNING) {
        tasks[current_task_idx].state = TASK_READY;
    }

    /* 3. Find the next READY normal task */
    int next_task_idx = -1;
    for (int i = 1; i < MAX_TASKS; i++) { 
        int idx = (current_task_idx + i) % (MAX_TASKS - 1); 
        if (tasks[idx].state == TASK_READY) {
            next_task_idx = idx;
            break;
        }
    }

    /* 4. Switch to Idle Task if no normal tasks are ready */
    if (next_task_idx == -1) {
        next_task_idx = MAX_TASKS - 1; 
    }

    /* 5. Perform switch */
    current_task_idx = next_task_idx;
    tasks[current_task_idx].state = TASK_RUNNING;
    
    return &tasks[current_task_idx].sp;
}

void os_delay(uint32_t ticks) {
    tasks[current_task_idx].wake_tick = os_tick_count + ticks;
    tasks[current_task_idx].state = TASK_SUSPENDED;
    os_yield(); 
}

void os_yield(void) {
    register uint32_t a7 __asm__("a7") = SYS_YIELD;
    __asm__ volatile ("ecall" : : "r"(a7));
}

void os_start_scheduler(void) {
    tasks[0].state = TASK_RUNNING;
    current_task_idx = 0;
    
    __asm__ volatile ("csrw mtvec, %0" :: "r"((uint32_t)vector_table | 1));
    __asm__ volatile ("csrw mscratch, %0" :: "r"(&tasks[0].sp));

    __asm__ volatile (
        "csrr t0, mscratch\n"
        "lw sp, 0(t0)\n"
        "lw t0, 32*4(sp)\n"
        "csrw mepc, t0\n"
        "lw t0, 33*4(sp)\n"
        "csrw mstatus, t0\n"
        "addi sp, sp, 144\n"
        "mret\n"
    );
}

void os_enter_critical(void) {
    register uint32_t a7 __asm__("a7") = SYS_ENTER_CRITICAL;
    __asm__ volatile ("ecall" : : "r"(a7));
}

void os_exit_critical(void) {
    register uint32_t a7 __asm__("a7") = SYS_EXIT_CRITICAL;
    __asm__ volatile ("ecall" : : "r"(a7));
}

/* Mutex Implementation */
void os_mutex_init(os_mutex_t *mutex) {
    mutex->locked = 0;
    mutex->owner_task_id = -1;
}

void os_mutex_take(os_mutex_t *mutex) {
    while (1) {
        os_enter_critical(); 
        
        if (mutex->locked == 0) {
            mutex->locked = 1;
            mutex->owner_task_id = current_task_idx;
            os_exit_critical();
            return; 
        }
        
        tasks[current_task_idx].state = TASK_BLOCKED;
        os_exit_critical(); 
        
        os_yield(); 
    }
}

void os_mutex_give(os_mutex_t *mutex) {
    os_enter_critical();
    
    if (mutex->locked == 1 && mutex->owner_task_id == current_task_idx) {
        mutex->locked = 0;
        mutex->owner_task_id = -1;
        
        /* Wake up all blocked tasks */
        for (int i = 0; i < MAX_TASKS; i++) {
            if (tasks[i].state == TASK_BLOCKED) {
                tasks[i].state = TASK_READY;
            }
        }
    }
    
    os_exit_critical();
}
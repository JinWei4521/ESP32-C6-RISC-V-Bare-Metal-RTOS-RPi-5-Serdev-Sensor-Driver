#include "timer.h"
#include <stdint.h>
#include "soc/systimer_reg.h"
#include "soc/interrupts.h"

extern void intr_matrix_set(int cpu_no, uint32_t model_num, uint32_t intr_num);
extern void esprv_int_set_priority(uint32_t intr_num, uint32_t prio);
extern void esprv_int_set_threshold(uint32_t threshold);
extern void esprv_int_enable(uint32_t intr_num);

#define REG_WRITE(r, v) (*(volatile uint32_t *)(r) = (v))
#define REG_READ(r) (*(volatile uint32_t *)(r))
#define REG_SET_BIT(r, b) (*(volatile uint32_t *)(r) |= (b))

void os_timer_init(void) {
    REG_SET_BIT(SYSTIMER_CONF_REG, SYSTIMER_CLK_EN);
    REG_SET_BIT(SYSTIMER_CONF_REG, SYSTIMER_TIMER_UNIT0_WORK_EN);

    /* Route SYSTIMER target 0 to CPU IRQ using the interrupt matrix */
    intr_matrix_set(0, ETS_SYSTIMER_TARGET0_INTR_SOURCE, CPU_IRQ);
    esprv_int_set_priority(CPU_IRQ, 15);
    esprv_int_set_threshold(0);
    esprv_int_enable(1 << CPU_IRQ);

    os_timer_reload(); 

    /* Enable global interrupts for this CPU IRQ */
    __asm__ volatile ("csrs mie, %0" :: "r"(1 << CPU_IRQ));
}

void os_timer_reload(void) {
    REG_WRITE(SYSTIMER_TARGET0_CONF_REG, 0); 
    REG_WRITE(SYSTIMER_INT_CLR_REG, SYSTIMER_TARGET0_INT_CLR);

    REG_WRITE(SYSTIMER_UNIT0_OP_REG, SYSTIMER_TIMER_UNIT0_UPDATE); 
    while(!(REG_READ(SYSTIMER_UNIT0_OP_REG) & SYSTIMER_TIMER_UNIT0_VALUE_VALID));
    
    uint64_t now = ((uint64_t)REG_READ(SYSTIMER_UNIT0_VALUE_HI_REG) << 32) | 
                    REG_READ(SYSTIMER_UNIT0_VALUE_LO_REG);
    
    uint64_t target = now + TICKS_100MS; 

    REG_WRITE(SYSTIMER_TARGET0_HI_REG, (uint32_t)(target >> 32));
    REG_WRITE(SYSTIMER_TARGET0_LO_REG, (uint32_t)(target & 0xFFFFFFFF));
    REG_WRITE(SYSTIMER_COMP0_LOAD_REG, SYSTIMER_TIMER_COMP0_LOAD); 
    REG_WRITE(SYSTIMER_TARGET0_CONF_REG, SYSTIMER_TARGET0_WORK_EN);
    REG_WRITE(SYSTIMER_INT_ENA_REG, SYSTIMER_TARGET0_INT_ENA); 
}
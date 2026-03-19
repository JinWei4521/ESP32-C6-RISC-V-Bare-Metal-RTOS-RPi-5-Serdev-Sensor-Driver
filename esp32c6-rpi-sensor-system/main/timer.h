#ifndef TIMER_H
#define TIMER_H

#define CPU_IRQ 16 
#define TICKS_100MS 1600000

void os_timer_init(void);
void os_timer_reload(void);

#endif // TIMER_H
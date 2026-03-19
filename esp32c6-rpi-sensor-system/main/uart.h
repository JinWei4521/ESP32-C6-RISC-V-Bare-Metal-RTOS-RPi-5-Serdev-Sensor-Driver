#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>
#include "task.h" // for os_mutex_t

#define CPU_INTR_UART 15
// Global UART mutex to synchronize access from multiple tasks and ISRs
extern os_mutex_t uart_mutex;

extern volatile bool sensor_enable_flag;

// Basic print functions for UART
// uart_num: 0 for UART0, 1 for UART1
void uart_putc(uint8_t uart_num, char c);
void uart_puts(uint8_t uart_num, const char *s);
void uart_putint(uint8_t uart_num, int val);
void uart_putfloat(uint8_t uart_num, float val);
void uart_puthex(uint8_t uart_num, uint32_t val);

void print_sensor_data(const char* name, float temp, const char* val2_name, float val2, const char* val2_unit);
void safe_print(const char* task_name, const char* msg);

void uart1_init(void);
void uart_interrupt_init(void);
void uart_rx_isr(void);

void bare_delay_us(uint32_t us);
void bare_delay_ms(uint32_t ms);

#endif // UART_H
#include <stdint.h>
#include "task.h"
#include "timer.h"
#include "soft_i2c.h"
#include "aht20.h"
#include "bmp280.h"
#include "uart.h"

#define READ_CSR(reg) ({ uint32_t _val; __asm__ volatile ("csrr %0, " #reg : "=r"(_val)); _val; })
#define WRITE_CSR(reg, val) ({ __asm__ volatile ("csrw " #reg ", %0" :: "rK"(val)); })

uint8_t stack_a[STACK_SIZE] __attribute__((aligned(16)));
uint8_t stack_b[STACK_SIZE] __attribute__((aligned(16)));
uint8_t stack_idle[STACK_SIZE] __attribute__((aligned(16)));

os_mutex_t i2c_mutex; 

void pmp_init(void) {
    __asm__ volatile ("csrw pmpcfg0, zero");

    /* Region 1: RAM / ROM / Flash (0x4000_0000 ~ 0x4FFF_FFFF, 256MB) */
    __asm__ volatile ("csrw pmpaddr0, %0" :: "r"(0x11FFFFFF));

    /* Region 2: Hardware Peripherals (0x6000_0000 ~ 0x600F_FFFF, 1MB) */
    __asm__ volatile ("csrw pmpaddr1, %0" :: "r"(0x1801FFFF));

    /* pmp0cfg = 0x1F (NAPOT, R/W/X), pmp1cfg = 0x1B (NAPOT, R/W) */
    __asm__ volatile ("csrw pmpcfg0, %0" :: "r"(0x00001B1F));
}

void task_aht20(void) {
    os_delay(1);
    aht20_init(); 

    float temp, humi;
    while (1) {
        if (sensor_enable_flag) {
            if (aht20_read(&temp, &humi) == OS_OK) {
                print_sensor_data("[AHT20]", temp, " Hum", humi, "%");
            } else {
                safe_print("[AHT20]", "Error");
            }
        }
        os_delay(10); 
    }
}

void task_bmp280(void) {
    os_delay(1); 

    while (bmp280_init_sensor() != OS_OK) {
        safe_print("[BMP280]", "Init Failed! Retrying...");
        os_delay(5); 
    }
    safe_print("[BMP280]", "Init Success!");

    float temp, press;
    while (1) {
        if (!sensor_enable_flag) {
            os_delay(10);
            continue;
        }
        
        if (bmp280_read_data(&temp, &press) == OS_OK) {
            print_sensor_data("[BMP280]", temp, " Press", press / 100.0f, "hPa");
        } else {
            safe_print("[BMP280]", "Read Error!");
        }
        os_delay(10); 
    }
}

void idle_task(void) {
    while (1) {
        os_yield();
    }
}

void c_trap_handler(void) {
    uint32_t mcause = READ_CSR(mcause);

    if (mcause & 0x80000000) {
        uint32_t exception_code = mcause & 0x7FFFFFFF;

        if (exception_code == CPU_IRQ) {
            os_tick_count++; 
            os_timer_reload();
            
            uint32_t** next_sp_addr = os_task_switch();
            WRITE_CSR(mscratch, next_sp_addr);
        }
        else if (exception_code == CPU_INTR_UART) {
            uart_rx_isr();
        }
    }
    else if (mcause == 8 || mcause == 11) { 
        uint32_t *current_sp;
        __asm__ volatile ("csrr %0, mscratch" : "=r"(current_sp));
        
        /* Read a7 (x17) from TrapFrame */
        uint32_t syscall_num = current_sp[17]; 

        /* Syscall Dispatcher */
        if (syscall_num == 0) { // SYS_YIELD
            uint32_t** next_sp_addr = os_task_switch();
            WRITE_CSR(mscratch, next_sp_addr);
        } 
        else if (syscall_num == 1) { // SYS_ENTER_CRITICAL
            __asm__ volatile ("csrci mstatus, 8");
        } 
        else if (syscall_num == 2) { // SYS_EXIT_CRITICAL
            __asm__ volatile ("csrsi mstatus, 8");
        }

        uint32_t *mepc_ptr = (uint32_t *)((uint32_t)*current_sp + (32 * 4));
        *mepc_ptr += 4; 

        uint32_t** next_sp_addr = os_task_switch();
        WRITE_CSR(mscratch, next_sp_addr);
    } 
    else {
        uint32_t mepc = READ_CSR(mepc);
        uint32_t mtval = READ_CSR(mtval);
        uart_puts(0, "\n[PANIC] Unhandled Trap! mcause: "); 
        uart_putint(0, mcause);
        uart_puts(0, "\n MEPC:  "); 
        uart_puthex(0, mepc);
        while(1);
    }
}

void app_main(void) {
    /* Disable interrupts initially */
    __asm__ volatile ("csrci mstatus, 8");
    __asm__ volatile ("csrw mie, zero"); 

    uart_puts(0, "\n--- Bare-Metal OS Kernel Boot ---\n");
    pmp_init();
    
    os_mutex_init(&uart_mutex);
    os_mutex_init(&i2c_mutex);
    
    i2c_init(I2C_SCL_PIN, I2C_SDA_PIN);
    uart1_init();
    uart_interrupt_init();

    os_task_create(0, stack_a, task_aht20);
    os_task_create(1, stack_b, task_bmp280);
    os_task_create(2, stack_idle, idle_task);

    os_timer_init();
    
    uart_puts(0, "Starting Scheduler: mret!\n");
    os_start_scheduler();

    while(1);
}
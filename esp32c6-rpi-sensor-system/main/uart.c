#include "uart.h"
#include "soc/interrupt_matrix_reg.h"
#include "soc/interrupts.h"
#include "riscv/interrupt.h"
#include "esp_rom_sys.h"
#include "soc/gpio_sig_map.h"
#include "soc/pcr_reg.h"
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"

#define UART0_BASE        0x60000000
#define UART0_FIFO        (UART0_BASE + 0x0000)

#define UART1_BASE        0x60001000
#define UART1_FIFO        (UART1_BASE + 0x0000)
#define UART1_INT_ST      (UART1_BASE + 0x0008) 
#define UART1_INT_ENA     (UART1_BASE + 0x000C) 
#define UART1_INT_CLR     (UART1_BASE + 0x0010) 
#define UART1_CLKDIV      (UART1_BASE + 0x0014)
#define UART1_CONF0       (UART1_BASE + 0x0020)
#define UART1_CONF1       (UART1_BASE + 0x0024)
#define UART1_CLK_CONF    (UART1_BASE + 0x0078)

#define PCR_BASE          0x60096000
#define PCR_UART1_CONF    (PCR_BASE + 0x000C)
#define PCR_UART1_SCLK    (PCR_BASE + 0x0010)

#define RXFIFO_FULL_INT_ENA  (1 << 0)
#define RXFIFO_FULL_INT_CLR  (1 << 0)

#define GET_UART_FIFO(num) ((num == 1) ? UART1_FIFO : UART0_FIFO)
#define UART_STATUS_REG(num) ((num == 1) ? (UART1_BASE + 0x001C) : (UART0_BASE + 0x001C))

os_mutex_t uart_mutex;
volatile bool sensor_enable_flag = true;

extern void intr_matrix_set(int cpu_no, uint32_t model_num, uint32_t intr_num);

void uart_putc(uint8_t uart_num, char c) {
    /* Check TXFIFO_CNT, wait if full (>= 127) */
    while (((*(volatile uint32_t *)UART_STATUS_REG(uart_num) >> 16) & 0xFF) >= 127) {
        __asm__ volatile("nop"); 
    }
    *(volatile uint32_t *)GET_UART_FIFO(uart_num) = c;
}

void uart_puts(uint8_t uart_num, const char *s) {
    while (*s) {
        uart_putc(uart_num, *s++);
    }
}

void uart_putint(uint8_t uart_num, int val) {
    char buf[16];
    int i = 0;
    if (val == 0) { uart_putc(uart_num, '0'); return; }
    if (val < 0) { uart_putc(uart_num, '-'); val = -val; }
    while (val > 0) { buf[i++] = (val % 10) + '0'; val /= 10; }
    while (i > 0) { uart_putc(uart_num, buf[--i]); }
}

void uart_puthex(uint8_t uart_num, uint32_t val) {
    uart_puts(uart_num, "0x");
    for (int i = 7; i >= 0; i--) {
        int nibble = (val >> (i * 4)) & 0xF;
        if (nibble < 10) uart_putc(uart_num, '0' + nibble);
        else uart_putc(uart_num, 'A' + nibble - 10);
    }
}

void uart_putfloat(uint8_t uart_num, float val) {
    int int_part = (int)val;
    int frac_part = (int)((val - int_part) * 100);
    if (frac_part < 0) frac_part = -frac_part;
    
    uart_putint(uart_num, int_part);
    uart_putc(uart_num, '.');
    if (frac_part < 10) uart_putc(uart_num, '0');
    uart_putint(uart_num, frac_part);
}

void print_sensor_data(const char* name, float temp, const char* val2_name, float val2, const char* val2_unit) {
    os_mutex_take(&uart_mutex);
    
    /* UART0: Human-readable format (Terminal) */
    uart_puts(0, name);
    uart_puts(0, " Temp: ");
    uart_putfloat(0, temp);
    uart_puts(0, " C, ");
    uart_puts(0, val2_name);
    uart_puts(0, ": ");
    uart_putfloat(0, val2);
    uart_puts(0, " ");
    uart_puts(0, val2_unit);
    uart_puts(0, "\n");

    /* UART1: CSV format for Raspberry Pi */
    uart_puts(1, name);
    uart_puts(1, ",");
    uart_putfloat(1, temp);
    uart_puts(1, "C,");
    uart_putfloat(1, val2);
    uart_puts(1, val2_unit);
    uart_puts(1, "\n"); 
    
    os_mutex_give(&uart_mutex);
}

void safe_print(const char* task_name, const char* msg) {
    os_mutex_take(&uart_mutex);
    uart_puts(0, task_name);
    uart_puts(0, ": ");
    uart_puts(0, msg);
    uart_puts(0, "\n");
    for (volatile int i = 0; i < 500000; i++); 
    os_mutex_give(&uart_mutex);
}

void uart1_init(void) {
    /* 1. Enable APB clock and clear reset */
    *(volatile uint32_t *)PCR_UART1_CONF_REG |= PCR_UART1_CLK_EN; 
    *(volatile uint32_t *)PCR_UART1_CONF_REG &= ~PCR_UART1_RST_EN;

    /* 2. Clone UART0 SCLK configuration */
    uint32_t u0_sclk = *(volatile uint32_t *)PCR_UART0_SCLK_CONF_REG;
    *(volatile uint32_t *)PCR_UART1_SCLK_CONF_REG = u0_sclk;
    *(volatile uint32_t *)PCR_UART1_SCLK_CONF_REG |= PCR_UART1_SCLK_EN;

    /* 3. Clone UART0 Baud Rate and frame format */
    *(volatile uint32_t *)UART1_CLK_CONF = *(volatile uint32_t *)(UART0_BASE + 0x0078);
    *(volatile uint32_t *)UART1_CLKDIV   = *(volatile uint32_t *)(UART0_BASE + 0x0014);
    *(volatile uint32_t *)UART1_CONF0    = *(volatile uint32_t *)(UART0_BASE + 0x0020);

    #define UART1_REG_UPDATE_REG (UART1_BASE + 0x0098)
    *(volatile uint32_t *)UART1_REG_UPDATE_REG |= (1 << 0);
    while (*(volatile uint32_t *)UART1_REG_UPDATE_REG & (1 << 0));
    
    /* 4. Configure GPIO pins (TX=22, RX=23) */
    int tx_pin = 22, rx_pin = 23;
    gpio_reset_pin(tx_pin);
    gpio_reset_pin(rx_pin);

    PIN_INPUT_ENABLE(IO_MUX_GPIO23_REG);
    gpio_set_direction(tx_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(rx_pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(rx_pin, GPIO_PULLUP_ENABLE);

    /* 5. Route internal signals */
    esp_rom_gpio_connect_out_signal(tx_pin, U1TXD_OUT_IDX, 0, 0);
    esp_rom_gpio_connect_in_signal(rx_pin, U1RXD_IN_IDX, 0);

    /* 6. Reset RX/TX FIFOs */
    #define UART1_CONF0_REG (UART1_BASE + 0x0020)
    *(volatile uint32_t *)UART1_CONF0_REG |= ((1 << 17) | (1 << 18));
    __asm__ volatile("nop; nop; nop; nop;");
    *(volatile uint32_t *)UART1_CONF0_REG &= ~((1 << 17) | (1 << 18));
}

void uart_interrupt_init(void) {
    /* 1. Set UART1 RX threshold to 1 byte */
    *(volatile uint32_t *)UART1_CONF1 = (*(volatile uint32_t *)UART1_CONF1 & ~0x3FF) | 1;

    #define UART1_REG_UPDATE_REG (UART1_BASE + 0x0098)
    *(volatile uint32_t *)UART1_REG_UPDATE_REG |= (1 << 0);
    while (*(volatile uint32_t *)UART1_REG_UPDATE_REG & (1 << 0));

    /* 2. Clear interrupts and enable RX FIFO full interrupt */
    *(volatile uint32_t *)UART1_INT_CLR = 0xFFFFFFFF;
    *(volatile uint32_t *)UART1_INT_ENA |= RXFIFO_FULL_INT_ENA;

    /* 3. Route to CPU IRQ line */
    intr_matrix_set(0, ETS_UART1_INTR_SOURCE, CPU_INTR_UART);
    esprv_int_set_priority(CPU_INTR_UART, 1);
    esprv_int_enable(1 << CPU_INTR_UART);

    /* 4. Enable CPU global interrupts for this line */
    __asm__ volatile ("csrs mie, %0" :: "r"(1 << CPU_INTR_UART));
}

void uart_rx_isr(void) {
    /* Read all available data from RX FIFO */
    while ((*(volatile uint32_t *)UART_STATUS_REG(1) & 0x3FF) > 0) {
        char c = *(volatile uint32_t *)UART1_FIFO;
        
        if (c == '0') {
            sensor_enable_flag = false;
            uart_puts(0, "\n[ISR] Sensors Disabled by Pi\n");
        } else if (c == '1') {
            sensor_enable_flag = true;
            uart_puts(0, "\n[ISR] Sensors Enabled by Pi\n");
        }
    }
    
    /* Clear interrupt flags */
    *(volatile uint32_t *)UART1_INT_CLR = 0xFFFFFFFF;
}

void bare_uart_puthex(uint32_t val) {
    uart_puts(0, "0x");
    for (int i = 7; i >= 0; i--) {
        int nibble = (val >> (i * 4)) & 0xF;
        if (nibble < 10) uart_putc(0, '0' + nibble);
        else uart_putc(0, 'A' + nibble - 10);
    }
}

void bare_delay_us(uint32_t us) {
    uint32_t loops = us * 40; 
    while (loops--) {
        __asm__ volatile("nop");
    }
}

void bare_delay_ms(uint32_t ms) {
    while (ms--) {
        bare_delay_us(1000);
    }
}
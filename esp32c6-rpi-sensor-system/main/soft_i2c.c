#include "soft_i2c.h"
#include "soc/gpio_reg.h" 
#include "task.h"         
#include "driver/gpio.h"
#include "uart.h"

extern os_mutex_t i2c_mutex; 

#define GPIO_BASE            0x60091000
#define GPIO_OUT_W1TC_REG    (GPIO_BASE + 0x000C) /* Write 1 to clear output (LOW) */
#define GPIO_ENABLE_W1TS_REG (GPIO_BASE + 0x0024) /* Write 1 to enable output */
#define GPIO_ENABLE_W1TC_REG (GPIO_BASE + 0x0028) /* Write 1 to disable output (HIGH/Float) */
#define GPIO_IN_REG          (GPIO_BASE + 0x003C) /* Read input level */

static int g_scl_pin = -1;
static int g_sda_pin = -1;

/* Microsecond delay using RISC-V cycle counter */
static inline void i2c_delay(void) {
    bare_delay_us(40);
}

#define REG_WRITE(r, v)   (*(volatile uint32_t *)(r) = (v))
#define REG_READ(r)       (*(volatile uint32_t *)(r))
#define REG_SET_BIT(r, b) (*(volatile uint32_t *)(r) |= (b))
#define REG_CLR_BIT(r, b) (*(volatile uint32_t *)(r) &= ~(b))

static inline void SDA_LOW(void) {
    REG_WRITE(GPIO_ENABLE_W1TS_REG, (1 << g_sda_pin));
}
static inline void SDA_HIGH(void) {
    REG_WRITE(GPIO_ENABLE_W1TC_REG, (1 << g_sda_pin));
}
static inline void SCL_LOW(void) {
    REG_WRITE(GPIO_ENABLE_W1TS_REG, (1 << g_scl_pin));
}
static inline void SCL_HIGH(void) {
    REG_WRITE(GPIO_ENABLE_W1TC_REG, (1 << g_scl_pin));
}
static inline int SDA_READ(void) { 
    return (REG_READ(GPIO_IN_REG) & (1 << g_sda_pin)) ? 1 : 0; 
}

void i2c_init(int scl_pin, int sda_pin) {
    g_scl_pin = scl_pin;
    g_sda_pin = sda_pin;

    gpio_reset_pin(g_scl_pin);
    gpio_reset_pin(g_sda_pin);

    /* 1. Set to bidirectional Input/Output mode */
    gpio_set_direction(g_scl_pin, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(g_sda_pin, GPIO_MODE_INPUT_OUTPUT);
    
    /* 2. Enable internal pull-up resistors */
    gpio_set_pull_mode(g_scl_pin, GPIO_PULLUP_ENABLE);
    gpio_set_pull_mode(g_sda_pin, GPIO_PULLUP_ENABLE);

    /* 3. Initial state: disable output, let bus float HIGH */
    SDA_HIGH();
    SCL_HIGH();
    
    /* 100ms initialization delay for sensors */
    bare_delay_ms(100); 
}

/* --- Basic I2C Atomic Operations --- */

void i2c_start(void) {
    SDA_HIGH(); SCL_HIGH(); i2c_delay();
    SDA_LOW(); i2c_delay();
    SCL_LOW();
}

void i2c_stop(void) {
    SDA_LOW(); i2c_delay();
    SCL_HIGH(); i2c_delay();
    SDA_HIGH(); i2c_delay();
}

void i2c_write_byte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        if (data & 0x80) SDA_HIGH();
        else SDA_LOW();
        data <<= 1;
        i2c_delay();
        
        SCL_HIGH(); i2c_delay();
        SCL_LOW();
    }
}

uint8_t i2c_read_byte(bool ack) {
    uint8_t data = 0;
    SDA_HIGH(); i2c_delay();

    for (int i = 0; i < 8; i++) {
        data <<= 1;
        SCL_HIGH(); i2c_delay(); 
        if (SDA_READ()) data |= 1;
        SCL_LOW(); i2c_delay();
    }

    if (ack) SDA_LOW();
    else SDA_HIGH();
    i2c_delay();
    
    SCL_HIGH(); i2c_delay();
    SCL_LOW(); SDA_HIGH(); 
    return data;
}

bool i2c_wait_ack(void) {
    SDA_HIGH(); i2c_delay();
    SCL_HIGH(); i2c_delay(); 
    bool ack = (SDA_READ() == 0); 
    SCL_LOW();
    return ack;
}

/* --- High-level API with Mutex Protection --- */

os_err_t i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data) {
    os_mutex_take(&i2c_mutex); 

    i2c_start();
    i2c_write_byte((dev_addr << 1) | 0);
    if (!i2c_wait_ack()) { i2c_stop(); os_mutex_give(&i2c_mutex); return OS_FAIL; }
    i2c_write_byte(reg_addr);
    if (!i2c_wait_ack()) { i2c_stop(); os_mutex_give(&i2c_mutex); return OS_FAIL; }
    i2c_write_byte(data);
    if (!i2c_wait_ack()) { i2c_stop(); os_mutex_give(&i2c_mutex); return OS_FAIL; }
    i2c_stop();

    os_mutex_give(&i2c_mutex); 
    return OS_OK;
}

os_err_t i2c_read_regs(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len) {
    os_mutex_take(&i2c_mutex);

    i2c_start();
    i2c_write_byte((dev_addr << 1) | 0);
    if (!i2c_wait_ack()) { i2c_stop(); os_mutex_give(&i2c_mutex); return OS_FAIL; }
    i2c_write_byte(reg_addr);
    if (!i2c_wait_ack()) { i2c_stop(); os_mutex_give(&i2c_mutex); return OS_FAIL; }
    i2c_start();
    i2c_write_byte((dev_addr << 1) | 1); 
    if (!i2c_wait_ack()) { i2c_stop(); os_mutex_give(&i2c_mutex); return OS_FAIL; }
    for (size_t i = 0; i < len; i++) {
        data[i] = i2c_read_byte(i < len - 1);
    }
    i2c_stop();

    os_mutex_give(&i2c_mutex); 
    return OS_OK;
}

os_err_t i2c_read_only(uint8_t dev_addr, uint8_t *data, size_t len) {
    os_mutex_take(&i2c_mutex);

    i2c_start();
    i2c_write_byte((dev_addr << 1) | 1); 
    if (!i2c_wait_ack()) { i2c_stop(); os_mutex_give(&i2c_mutex); return OS_FAIL; }
    for (size_t i = 0; i < len; i++) {
        data[i] = i2c_read_byte(i < len - 1);
    }
    i2c_stop();

    os_mutex_give(&i2c_mutex);
    return OS_OK;
}
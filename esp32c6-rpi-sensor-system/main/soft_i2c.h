#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    OS_OK = 0,
    OS_FAIL = -1,
    OS_TIMEOUT = -2,
    I2C_read1 = 1,
    I2C_read2 = 2,
    ID = 3,
} os_err_t;

#define I2C_SDA_PIN 10
#define I2C_SCL_PIN 11

// Initialize software I2C with specified SCL and SDA pins
void i2c_init(int scl_pin, int sda_pin);

// for debug: quick scan I2C bus for devices
void i2c_scan(void);

// automatically generate start, stop, write_byte, read_byte, wait_ack functions
void i2c_start(void);
void i2c_stop(void);
void i2c_write_byte(uint8_t data);
uint8_t i2c_read_byte(bool ack);
bool i2c_wait_ack(void);

// High-level API with Mutex protection
os_err_t i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
os_err_t i2c_read_regs(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len);
os_err_t i2c_read_only(uint8_t dev_addr, uint8_t *data, size_t len);

#endif
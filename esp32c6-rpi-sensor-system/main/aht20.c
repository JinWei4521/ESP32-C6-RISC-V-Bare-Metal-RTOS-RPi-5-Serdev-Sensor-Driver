#include "aht20.h"
#include "task.h"

extern os_mutex_t i2c_mutex;

void aht20_init(void) {
    os_mutex_take(&i2c_mutex); 
    
    i2c_start();
    i2c_write_byte((AHT20_ADDR << 1) | 0);
    if(i2c_wait_ack()) {
        i2c_write_byte(0xBE); i2c_wait_ack();
        i2c_write_byte(0x08); i2c_wait_ack();
        i2c_write_byte(0x00); i2c_wait_ack();
    }
    i2c_stop();
    
    os_mutex_give(&i2c_mutex); 
    os_delay(1); 
}

os_err_t aht20_read(float *t, float *h) {
    /* 1. Send measurement command */
    os_mutex_take(&i2c_mutex);
    i2c_start();
    i2c_write_byte((AHT20_ADDR << 1) | 0);
    if (!i2c_wait_ack()) { i2c_stop(); os_mutex_give(&i2c_mutex); return OS_FAIL; }
    i2c_write_byte(0xAC); i2c_wait_ack();
    i2c_write_byte(0x33); i2c_wait_ack();
    i2c_write_byte(0x00); i2c_wait_ack();
    i2c_stop();
    os_mutex_give(&i2c_mutex);


    /* 2. Read data */
    uint8_t data[6];
    if (i2c_read_only(AHT20_ADDR, data, 6) != OS_OK) return OS_FAIL;
    
    uint32_t raw_h = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    uint32_t raw_t = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
    *h = (raw_h * 100.0f) / 1048576.0f;
    *t = (raw_t * 200.0f) / 1048576.0f - 50.0f;
    
    return OS_OK;
}
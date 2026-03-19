#ifndef BMP280_H
#define BMP280_H

#include "soft_i2c.h"

#define BMP280_ADDR 0x77

typedef struct {
    uint16_t dig_T1; int16_t dig_T2; int16_t dig_T3;
    uint16_t dig_P1; int16_t dig_P2; int16_t dig_P3;
    int16_t dig_P4;  int16_t dig_P5; int16_t dig_P6;
    int16_t dig_P7;  int16_t dig_P8; int16_t dig_P9;
} bmp280_cal_t;

os_err_t bmp280_init_sensor(void);
os_err_t bmp280_read_data(float *temp, float *pressure);

#endif
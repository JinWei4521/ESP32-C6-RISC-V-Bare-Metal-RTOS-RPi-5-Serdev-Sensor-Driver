#ifndef AHT20_H
#define AHT20_H

#include "soft_i2c.h"

#define AHT20_ADDR 0x38

void aht20_init(void);
os_err_t aht20_read(float *t, float *h);

#endif
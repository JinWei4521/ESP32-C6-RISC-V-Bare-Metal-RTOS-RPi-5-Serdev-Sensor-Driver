#include "bmp280.h"
#include "task.h"

extern os_mutex_t i2c_mutex;

static bmp280_cal_t cal;
static int32_t t_fine;

os_err_t bmp280_init_sensor(void) {
    uint8_t id;
    if (i2c_read_regs(BMP280_ADDR, 0xD0, &id, 1) != OS_OK) return I2C_read1;
    if (id != 0x58 && id != 0x60) return ID;

    uint8_t cal_data[24];
    if (i2c_read_regs(BMP280_ADDR, 0x88, cal_data, 24) != OS_OK) return I2C_read2;

    cal.dig_T1 = (cal_data[1] << 8) | cal_data[0];
    cal.dig_T2 = (int16_t)((cal_data[3] << 8) | cal_data[2]);
    cal.dig_T3 = (int16_t)((cal_data[5] << 8) | cal_data[4]);
    cal.dig_P1 = (cal_data[7] << 8) | cal_data[6];
    cal.dig_P2 = (int16_t)((cal_data[9] << 8) | cal_data[8]);
    cal.dig_P3 = (int16_t)((cal_data[11] << 8) | cal_data[10]);
    cal.dig_P4 = (int16_t)((cal_data[13] << 8) | cal_data[12]);
    cal.dig_P5 = (int16_t)((cal_data[15] << 8) | cal_data[14]);
    cal.dig_P6 = (int16_t)((cal_data[17] << 8) | cal_data[16]);
    cal.dig_P7 = (int16_t)((cal_data[19] << 8) | cal_data[18]);
    cal.dig_P8 = (int16_t)((cal_data[21] << 8) | cal_data[20]);
    cal.dig_P9 = (int16_t)((cal_data[23] << 8) | cal_data[22]);

    i2c_write_reg(BMP280_ADDR, 0xF5, 0xA0); /* Standby 500ms, Filter x16 */
    i2c_write_reg(BMP280_ADDR, 0xF4, 0x57); /* Normal Mode */
    
    return OS_OK;
}

os_err_t bmp280_read_data(float *temp, float *pressure) {
    uint8_t data[6];
    
    /* i2c_read_regs internally handles mutex locking */
    if (i2c_read_regs(BMP280_ADDR, 0xF7, data, 6) != OS_OK) return OS_FAIL;

    int32_t adc_p = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    int32_t adc_t = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);

    /* --- Temperature Calculation --- */
    int32_t var1 = ((((adc_t >> 3) - ((int32_t)cal.dig_T1 << 1))) * ((int32_t)cal.dig_T2)) >> 11;
    int32_t var2 = (((((adc_t >> 4) - ((int32_t)cal.dig_T1)) * ((adc_t >> 4) - ((int32_t)cal.dig_T1))) >> 12) * ((int32_t)cal.dig_T3)) >> 14;
    t_fine = var1 + var2;
    *temp = ((t_fine * 5 + 128) >> 8) / 100.0f;

    /* --- Pressure Calculation --- */
    int64_t p_var1, p_var2, p;
    p_var1 = ((int64_t)t_fine) - 128000;
    p_var2 = p_var1 * p_var1 * (int64_t)cal.dig_P6;
    p_var2 = p_var2 + ((p_var1 * (int64_t)cal.dig_P5) << 17);
    p_var2 = p_var2 + (((int64_t)cal.dig_P4) << 35);
    p_var1 = ((p_var1 * p_var1 * (int64_t)cal.dig_P3) >> 8) + ((p_var1 * (int64_t)cal.dig_P2) << 12);
    p_var1 = (((((int64_t)1) << 47) + p_var1)) * ((int64_t)cal.dig_P1) >> 33;

    if (p_var1 == 0) {
        *pressure = 0;
    } else {
        p = 1048576 - adc_p;
        p = (((p << 31) - p_var2) * 3125) / p_var1;
        p_var1 = (((int64_t)cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
        p_var2 = (((int64_t)cal.dig_P8) * p) >> 19;
        p = ((p + p_var1 + p_var2) >> 8) + (((int64_t)cal.dig_P7) << 4);
        *pressure = (float)p / 256.0f;
    }
    
    return OS_OK;
}
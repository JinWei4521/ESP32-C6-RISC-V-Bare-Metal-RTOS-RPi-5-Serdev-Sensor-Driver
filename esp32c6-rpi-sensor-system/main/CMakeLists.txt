idf_component_register(SRCS "uart.c" "bmp280.c" "aht20.c" "soft_i2c.c" "task.c" "timer.c" "main.c" "trap_handler.S"
                       REQUIRES soc esp_rom driver
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS "")

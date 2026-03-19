#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <pthread.h>
#include "c6_driver.h"

int fd;

/* Thread function to continuously read sensor data in the background */
void *read_sensor_thread(void *arg) {
    char buf[128];
    char line_buf[256];
    int line_pos = 0;
    int bytes_read;

    while (1) {
        bytes_read = read(fd, buf, sizeof(buf) - 1);
        if (bytes_read > 0) {
            /* Process received fragmented data byte by byte */
            for (int i = 0; i < bytes_read; i++) {
                if (buf[i] == '\n' || buf[i] == '\r') {
                    if (line_pos > 0) {
                        line_buf[line_pos] = '\0';
                        printf("\r\033[K[Data Received] %s\n> Command (1=Enable, 0=Disable, q=Quit): ", line_buf);
                        fflush(stdout);
                        line_pos = 0;
                    }
                } else {
                    if (line_pos < sizeof(line_buf) - 1) {
                        line_buf[line_pos++] = buf[i];
                    }
                }
            }
        }
    }
    return NULL;
}

int main() {
    pthread_t read_tid;
    char input[10];

    printf("Opening sensor device...\n");
    fd = open("/dev/tty_sensor", O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }
    printf("Successfully opened /dev/tty_sensor.\n");

    /* Start the background reading thread */
    pthread_create(&read_tid, NULL, read_sensor_thread, NULL);

    /* Main thread: handle user input */
    while (1) {
        printf("> Command (1=Enable, 0=Disable, q=Quit): ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) break;

        if (input[0] == '1') {
            ioctl(fd, SENSOR_IOC_ENABLE);
            printf("[INFO] Sent ENABLE command to ESP32-C6\n");
        } else if (input[0] == '0') {
            ioctl(fd, SENSOR_IOC_DISABLE);
            printf("[INFO] Sent DISABLE command to ESP32-C6\n");
        } else if (input[0] == 'q') {
            printf("Exiting test program.\n");
            break;
        } else {
            printf("[ERROR] Invalid command. Please enter 1, 0, or q.\n");
        }
    }

    close(fd);
    return 0;
}
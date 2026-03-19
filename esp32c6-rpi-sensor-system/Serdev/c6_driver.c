#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/serdev.h>
#include <linux/mod_devicetable.h>
#include "c6_driver.h"

#define DRIVER_NAME "esp32_sensor"
#define RX_BUF_SIZE 256

/* Private device data structure */
struct sensor_device_data {
    struct cdev cdev;
    struct serdev_device *serdev;
    
    spinlock_t lock;
    wait_queue_head_t wait_q;
    bool has_data;
    
    char rx_buffer[RX_BUF_SIZE];
    int rx_length;
};

static struct sensor_device_data *sensor_data;
static dev_t dev_num;

/* Serdev receive callback */
static ssize_t sensor_serdev_receive_buf(struct serdev_device *serdev, const u8 *buf, size_t count) {
    unsigned long flags;
    int copy_len;

    spin_lock_irqsave(&sensor_data->lock, flags);
    
    copy_len = min((int)count, RX_BUF_SIZE - sensor_data->rx_length);
    memcpy(&sensor_data->rx_buffer[sensor_data->rx_length], buf, copy_len);
    sensor_data->rx_length += copy_len;
    sensor_data->has_data = true;
    
    spin_unlock_irqrestore(&sensor_data->lock, flags);
    wake_up_interruptible(&sensor_data->wait_q);

    return count;
}

static const struct serdev_device_ops sensor_serdev_ops = {
    .receive_buf = sensor_serdev_receive_buf,
    .write_wakeup = serdev_device_write_wakeup,
};

/* Character device read operation */
static ssize_t sensor_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
    unsigned long flags;
    int bytes_to_copy;
    char temp_buf[RX_BUF_SIZE];

    if (wait_event_interruptible(sensor_data->wait_q, sensor_data->has_data))
        return -ERESTARTSYS;

    spin_lock_irqsave(&sensor_data->lock, flags);

    bytes_to_copy = min((int)count, sensor_data->rx_length);
    memcpy(temp_buf, sensor_data->rx_buffer, bytes_to_copy);

    /* Shift remaining data to the front of the buffer */
    if (bytes_to_copy < sensor_data->rx_length) {
        int remaining = sensor_data->rx_length - bytes_to_copy;
        memmove(sensor_data->rx_buffer, &sensor_data->rx_buffer[bytes_to_copy], remaining);
        sensor_data->rx_length = remaining;
    } else {
        sensor_data->rx_length = 0;
        sensor_data->has_data = false;
    }

    spin_unlock_irqrestore(&sensor_data->lock, flags);

    if (copy_to_user(user_buf, temp_buf, bytes_to_copy))
        return -EFAULT;

    return bytes_to_copy;
}

static long sensor_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch(cmd) {
        case SENSOR_IOC_ENABLE:
            serdev_device_write(sensor_data->serdev, "1", 1, MAX_SCHEDULE_TIMEOUT);
            // Wait until data is fully transmitted to the hardware
            // serdev_device_wait_until_sent(sensor_data->serdev, 0); 
            pr_info("Sensor: Sent ENABLE command to ESP32\n");
            break;

        case SENSOR_IOC_DISABLE:
            serdev_device_write(sensor_data->serdev, "0", 1, MAX_SCHEDULE_TIMEOUT);
            serdev_device_wait_until_sent(sensor_data->serdev, 0);
            pr_info("Sensor: Sent DISABLE command to ESP32\n");
            break;

        default:
            return -ENOTTY;
    }
    return 0;
}

static int sensor_open(struct inode *inode, struct file *file) { return 0; }
static int sensor_release(struct inode *inode, struct file *file) { return 0; }

static struct file_operations sensor_fops = {
    .owner = THIS_MODULE,
    .open = sensor_open,
    .release = sensor_release,
    .read = sensor_read,
    .unlocked_ioctl = sensor_ioctl,
};

static int sensor_serdev_probe(struct serdev_device *serdev) {
    int ret;

    pr_info("Sensor: Device found, probing...\n");

    sensor_data = devm_kzalloc(&serdev->dev, sizeof(*sensor_data), GFP_KERNEL);
    if (!sensor_data) return -ENOMEM;

    sensor_data->serdev = serdev;
    spin_lock_init(&sensor_data->lock);
    init_waitqueue_head(&sensor_data->wait_q);

    serdev_device_set_client_ops(serdev, &sensor_serdev_ops);
    ret = serdev_device_open(serdev);
    if (ret) return ret;
    
    serdev_device_set_baudrate(serdev, 115200);
    serdev_device_set_flow_control(serdev, false);

    cdev_init(&sensor_data->cdev, &sensor_fops);
    ret = cdev_add(&sensor_data->cdev, dev_num, 1);
    if (ret) goto close_serdev;

    pr_info("Sensor: Probe successful. UART Baudrate: 115200\n");
    return 0;

close_serdev:
    serdev_device_close(serdev);
    return ret;
}

static void sensor_serdev_remove(struct serdev_device *serdev) {
    cdev_del(&sensor_data->cdev);
    serdev_device_close(serdev);
    pr_info("Sensor: Device removed\n");
}

static const struct of_device_id sensor_of_match[] = {
    { .compatible = "mycompany,esp32-sensor", },
    { }
};
MODULE_DEVICE_TABLE(of, sensor_of_match);

static struct serdev_device_driver sensor_serdev_driver = {
    .probe = sensor_serdev_probe,
    .remove = sensor_serdev_remove,
    .driver = {
        .name = "esp32_sensor_driver",
        .of_match_table = sensor_of_match,
    },
};

static int __init sensor_init(void) {
    alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME);
    return serdev_device_driver_register(&sensor_serdev_driver);
}

static void __exit sensor_exit(void) {
    serdev_device_driver_unregister(&sensor_serdev_driver);
    unregister_chrdev_region(dev_num, 1);
}

module_init(sensor_init);
module_exit(sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("ESP32-C6 UART Sensor Serdev Driver");
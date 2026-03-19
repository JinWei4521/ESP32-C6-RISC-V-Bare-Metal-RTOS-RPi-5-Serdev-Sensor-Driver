#ifndef __SENSOR_IOCTL_H
#define __SENSOR_IOCTL_H

#include <linux/ioctl.h>

/*  Magic Number and Hardware Control Commands */
#define SENSOR_MAGIC 'S'
#define SENSOR_IOC_ENABLE  _IO(SENSOR_MAGIC, 1)
#define SENSOR_IOC_DISABLE _IO(SENSOR_MAGIC, 0)

#endif // __SENSOR_IOCTL_H

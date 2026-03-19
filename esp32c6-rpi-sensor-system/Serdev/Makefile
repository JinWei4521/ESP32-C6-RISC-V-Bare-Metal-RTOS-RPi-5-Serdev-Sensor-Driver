CFLAGS += -Wall
CFILES = c6_driver.c

obj-m += c6_driver.o
driver-objs := $(CFILES:.c=.o)

# Get the header files and build environment path for the currently running kernel
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

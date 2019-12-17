obj-m = nas_pm.o
nas_pm-objs += main.o page.o
KVERSION = $(shell uname -r)

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

start:
	dmesg -C
	insmod nas_pm.ko
	dmesg

stop:
	rmmod nas_pm.ko

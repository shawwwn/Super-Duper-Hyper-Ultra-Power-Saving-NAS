obj-m = nas_pm.o
nas_pm-objs += main.o page.o util.o thread.o gpio.o
KVERSION = $(shell uname -r)

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

start:
	dmesg -C
	insmod nas_pm.ko uuid="000D0B09000962AD" mountpoint="/media/usb2"
	dmesg

stop:
	rmmod nas_pm.ko

obj-m = nas_pm.o
KVERSION = $(shell uname -r)

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

test: all
	dmesg -C
	insmod nas_pm.ko
	dmesg

stop:
	rmmod nas_pm.ko

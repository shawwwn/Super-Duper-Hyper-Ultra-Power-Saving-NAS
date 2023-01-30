obj-m = nas_pm.o
nas_pm-objs += main.o page.o util.o thread.o gpio.o patch.o
KVERSION = $(shell uname -r)
CFLAGS_main.o := -Wno-format-extra-args

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

install:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules_install
	@cp -avr etc /
	depmod
	@echo "Done."

uninstall:
	rm -f /lib/modules/$(KVERSION)/extra/nas_pm.ko*
	rm -f /etc/modprobe.d/nas_pm.conf
	rm -f /etc/modules-load.d/nas_pm.conf
	rm -f /etc/nas_pm/mountscript.sh

start:
	dmesg -C
	insmod nas_pm.ko uuid="000D0B09000962AD" mountpoint="/media/usb2" gpio=488
	dmesg

stop:
	rmmod nas_pm.ko

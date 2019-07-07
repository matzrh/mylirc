obj-m += pwm-ir-tx.o
obj-m += gpio-nocarrier-ir-tx.o
KDIR=/lib/modules/`uname -r`/build
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

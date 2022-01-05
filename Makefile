CFILES = systemcall.c memory_leak.c fork.c signal.c ipi.c fair.c

obj-m += system.o
system-objs := $(CFILES:.c=.o)

KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) modules clean

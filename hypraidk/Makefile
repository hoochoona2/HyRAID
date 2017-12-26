obj-m := hypraidk.o
hypraidk-objs := hypraidk-main.o bio_collector.o ioctl.o hypraidk_lib.o configure.o bitmap.o block_area.o replacement.o
MY_CFLAGS += -g -DDEBUG -O0
ccflags-y += ${MY_CFLAGS}
CC += ${MY_CFLAGS}

all:
	    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules


clean:
	    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

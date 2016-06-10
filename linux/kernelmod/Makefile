ccflags-y := -std=gnu99 -Wno-declaration-after-statement

obj-m += fifogfx.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

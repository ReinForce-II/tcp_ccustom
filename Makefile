obj-m += tcp_ccustom.o

all:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
		install tcp_ccustom.ko /lib/modules/$(shell uname -r)/kernel/net/ipv4
		insmod /lib/modules/$(shell uname -r)/kernel/net/ipv4/tcp_ccustom.ko

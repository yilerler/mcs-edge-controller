# 指定要編譯的模組名稱
obj-m += src/mock_elc_core.o

# 自動抓取目前的作業系統核心版本
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
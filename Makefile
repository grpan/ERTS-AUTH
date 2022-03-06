CC = gcc
CC_ARM = cross-pi-gcc-10.3.0-0/bin/arm-linux-gnueabihf-gcc
CFLAGS =  -pthread -Wall -Wformat=2 -Wundef -Wextra -pedantic -Wunused-macros -Wshadow -fno-common -Wpadded

.PHONY: clean

UPLOAD ?= 0
DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CFLAGS += -ggdb3 -O0
else
    CFLAGS += -O3
endif

SRCS := $(wildcard *.c)
OBJS := $(wildcard *.o)
BINS := $(SRCS:%.c=%)
 
covidTrace:
	@echo Compiling covidTrace for testing...
	${CC} ${CFLAGS} $@.c -o $@

covidTrace_ARM32:
	@echo Compiling for ARM32 \(rpi zero W\)...
	$(CC_ARM) ${CFLAGS} covidTrace.c -o $@
	@if [ $(UPLOAD) = 1 ]; then\
		echo "Uploading file to espx...";\
		sshpass -p "espx2019" scp covidTrace_ARM32 root@192.168.0.1:/home;\
    fi


clean:
	@echo Removing files: $(BINS) $(OBJS)
	rm -f $(BINS) $(OBJS) covidTrace_ARM32

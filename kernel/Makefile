NAME           := kernel
SRCS           := framework/main.c $(shell find -L ./src/ -name "*.c")
INC_PATH       := include/ framework/
CFLAGS += -DTEST1 -DDEBUG
#CFLAGS += -DTEST1
export MODULE := L2

export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH    := x86_64-qemu

#export ARCH    := native
export smp := 4
endif

include $(AM_HOME)/Makefile
include ../oslabs.mk
image: git

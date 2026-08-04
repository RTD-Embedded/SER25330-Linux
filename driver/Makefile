#
#	FILE NAME: Makefile
#
#	FILE DESCRIPTION: Make description file for building example programs
#
#	PROJECT NAME: Linux Software (SER25330)
#
#	PROJECT VERSION: (Defined in README.TXT)
#
#	This file and its contents are copyright (C) RTD Embedded Technologies,
#	Inc.  All Rights Reserved.
#
#	This software is licensed as described in the RTD End-User Software
#	License Agreement.  For a copy of this agreement, refer to the file
#	LICENSE.TXT (which should be included with this software) or contact RTD
#	Embedded Technologies, Inc.
#


#
CC=gcc
DEBUG_FLAGS=
INCLUDE_FLAGS=-I../include
LIBRARY_FLAGS=-L../lib -lrtd-ser25330
OPTIMIZE_FLAGS=-O6
WARNING_FLAGS=-Wall

MY_CFLAGS := -I$(shell pwd)/../include -g

#
# Kernel build environment directory.  Supposedly it is safer to use this
# method of referring to it than using /usr/src.
#

KERNEL_DIR := /lib/modules/$(shell uname -r)/build

obj-m := exar_rtd.o

DRIVER_MODULE=exar_rtd.ko
FILES_TO_CLEAN= \
	*.o \
	exar_rtd.ko \
	.*.cmd \
	*.ko \
	.tmp_versions \
	exar_rtd.mod.c \
	.*.d \
	*.symvers \
	*.order \
	*.markers \
	*~ \
	exar*.mod \
	.exar*



driver:		exar_rtd.c
	make -C $(KERNEL_DIR) M=`pwd` EXTRA_CFLAGS="$(MY_CFLAGS)" modules


load:
	insmod ./$(DRIVER_MODULE)
	
clean:
	rm -rf $(FILES_TO_CLEAN)  


# M-Stack Bootloader Makefile
#
# This file may be used under the terms of the Simplified BSD License
# (2-clause), which can be found in LICENSE-bsd.txt in the parent
# directory.
#
# It is worth noting that M-Stack itself is not under the same license as
# this file.  See the top-level README.txt for more information.
#
# Alan Ott
# Signal 11 Software

# This Makefile makes heavy use of GNU Make's implicit rules. On non-GNU
# platforms, make sure to use gmake to build this project
#
OBJS= hex.o bootloader.o main.o
CFLAGS= -g -Wall
CXXFLAGS= $(CFLAGS)
LDFLAGS= -g
LDLIBS=

# Debugging: See log.h
#CFLAGS+= -DDEBUG
#CFLAGS+= -DLOG_LIBUSB_ERRORS
#CFLAGS+= -DLOG_HEX

OS=$(shell uname)
ifneq ($(OS), Darwin)
LDLIBS+=`pkg-config --libs libusb-1.0`
CFLAGS+=`pkg-config --cflags libusb-1.0`
else
LDLIBS+=-framework IOKit -framework CoreFoundation
endif

all: bootloader

bootloader: $(OBJS)
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f $(OBJS) bootloader

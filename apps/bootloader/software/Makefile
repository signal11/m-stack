# Makefile for Bootloader.
# This Makefile makes heavy use of GNU Make's implicit rules. On non-GNU
# platforms, make sure to use gmake to build this project
#
OBJS= hex.o bootloader.o main.o
CFLAGS= -g -Wall
CXXFLAGS= $(CFLAGS)
LDFLAGS= -g
LDLIBS=

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

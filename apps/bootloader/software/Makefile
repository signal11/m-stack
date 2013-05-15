# Makefile for Bootloader.
# This Makefile makes heavy use of GNU Make's implicit rules. On non-GNU
# platforms, make sure to use gmake to build this project
#
SO_OBJS= hex.o
MAIN_OBJ= main.o
CFLAGS= -g -Wall #-fPIC -fvisibility=hidden
CXXFLAGS= $(CFLAGS)
LDFLAGS= -g
LDLIBS= 
SO_LDFLAGS= -shared

OS=$(shell uname)
ifneq ($(OS), Darwin)
SO_OBJS+=
LDLIBS+=`pkg-config --libs libusb-1.0`
CFLAGS+=`pkg-config --cflags libusb-1.0`
else
SO_OBJS+=
LDLIBS+=-framework IOKit -framework CoreFoundation
endif

OBJS=$(SO_OBJS) $(MAIN_OBJ)


all: bootloader #libbootloader.so bootloader-dynamic

bootloader: $(OBJS)
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

#libbootloader.so: $(SO_OBJS)
#	$(CXX) $^ $(LDFLAGS) $(SO_LDFLAGS) $(LDLIBS) -o $@

#bootloader-dynamic: $(MAIN_OBJ)
#	$(CXX) $^ $(LDFLAGS) -L. -lbootloader $(LDLIBS) -o $@

clean:
	rm -f $(OBJS) bootloader libbootloader.so bootloader-dynamic

# M-Stack Host Test Software Makefile
#
# This file may be used by anyone for any purpose and may be used as a
# starting point making your own application using M-Stack.
#
# It is worth noting that M-Stack itself is not under the same license as
# this file.  See the top-level README.txt for more information.
#
# Alan Ott
# Signal 11 Software

all: test feature feature_test control_transfer_out control_transfer_in

test: test.c
	gcc -Wall -g -o test test.c `pkg-config libusb-1.0 --cflags --libs`

feature: feature.c
	gcc -Wall -g -o feature feature.c `pkg-config libusb-1.0 --cflags --libs`

feature_test: feature_test.c
	gcc -Wall -g -o feature_test feature_test.c `pkg-config libusb-1.0 --cflags --libs`

control_transfer_out: control_transfer_out.c
	gcc -Wall -g -o control_transfer_out control_transfer_out.c `pkg-config libusb-1.0 --cflags --libs`

control_transfer_in: control_transfer_in.c
	gcc -Wall -g -o control_transfer_in control_transfer_in.c `pkg-config libusb-1.0 --cflags --libs`


all: test feature control_transfer_out control_transfer_in

test: test.c
	gcc -Wall -g -o test test.c `pkg-config libusb-1.0 --cflags --libs`

feature: feature.c
	gcc -Wall -g -o feature feature.c `pkg-config libusb-1.0 --cflags --libs`

control_transfer_out: control_transfer_out.c
	gcc -Wall -g -o control_transfer_out control_transfer_out.c `pkg-config libusb-1.0 --cflags --libs`

control_transfer_in: control_transfer_in.c
	gcc -Wall -g -o control_transfer_in control_transfer_in.c `pkg-config libusb-1.0 --cflags --libs`

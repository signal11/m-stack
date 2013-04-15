
all: test feature

test: test.c
	gcc -Wall -g -o test test.c `pkg-config libusb-1.0 --cflags --libs`

feature: feature.c
	gcc -Wall -g -o feature feature.c `pkg-config libusb-1.0 --cflags --libs`


FILES := engine.c bootd.c util.c protocol.c usb_linux.c bootimg.c


all: 
	gcc -g -std=gnu99 $(FILES)

FILES := engine.c bootd.c util.c protocol.c usb_linux.c bootimg.c
PREFIX=/usr/local

all: fastbootd

fastbootd: $(FILES) 
	$(CC) -std=gnu99 $(FILES) -o $(@)

install: fastbootd
	cp fastboot $(PREFIX)/bin

clean:
	-rm fastbootd

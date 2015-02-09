
FILES := engine.c bootd.c util.c protocol.c usb_linux.c bootimg.c

all: fastbootd

fastbootd: $(FILES) 
	$(CC) -std=gnu99 $(FILES) -o $(@)

clean:
	-rm fastbootd

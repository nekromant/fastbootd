#fastbootd - A heavily stripped down fastboot server

This is a hacky stripped down fastboot server that sits there and pushes kernel, optional initrd 
and/or secondstage to the board when it detects one. It requires no android buildsystem whatsoever, 
no dependencies can be easily cross-compiled.
Simple as that, no other modes. 


# Compiling & installing

```
make
sudo make install
```

# Using

Without initrd:

```
fastbootd uImage
```

Or with zImage
```
fastbootd zImage
```

With initrd
```
fastbootd zImage initrd.bin
```

With initrd and secondstage
```
fastbootd zImage initrd.bin secondstage.bin
```

# The story. 

The story behind is: I needed something to push linux kernel over fastboot usb to 
odroid-x2 boards whenever they rebooted. Since odroid has shitty network and cdc_ether looked better,
I needed a tftp replacement. Fastboot was the only option here. And I needed the solution to work on an
OpenWRT box. 

Unfortunately android's fastboot utility is a huge pile of smoking crap that can't be
built outside anroid buidsystem. Moreover, original fastboot has a whole load of weird dependencies 
and strange modes. 

Srsly, people at google, if you're reading this - google the words "KISS" and "unix-way". 

To make things even worse, they tend to malloc memory and NOT free it, since it's not designed to 
run as a daemon. I wasted a few hours to clean it up a little. That resulted in dropping most of 
weird modes, leaving only the basics I needed. I may not have cleaned all the memory leaks, so fastbootd
just execl's itself after each run.  
 


 
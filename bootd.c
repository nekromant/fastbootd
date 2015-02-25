#define _LARGEFILE64_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "fastboot.h"
#include "bootimg.h"

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))

char cur_product[FB_RESPONSE_SZ + 1];

void bootimg_set_cmdline(boot_img_hdr *h, const char *cmdline);

boot_img_hdr *mkbootimg(void *kernel, unsigned kernel_size, unsigned kernel_offset,
                        void *ramdisk, unsigned ramdisk_size, unsigned ramdisk_offset,
                        void *second, unsigned second_size, unsigned second_offset,
                        unsigned page_size, unsigned base, unsigned tags_offset,
                        unsigned *bootimg_size);


static const char *serial = 0;
static const char *product = 0;
static unsigned short vendor_id = 0;

unsigned page_size = 2048;
unsigned base_addr      = 0x10000000;
unsigned kernel_offset  = 0x00008000;
unsigned ramdisk_offset = 0x01000000;
unsigned second_offset  = 0x00f00000;
unsigned tags_offset    = 0x00000100;



int match_fastboot_with_serial(usb_ifc_info *info, const char *local_serial)
{
    if(!(vendor_id && (info->dev_vendor == vendor_id)) &&
       (info->dev_vendor != 0x18d1) &&  // Google
       (info->dev_vendor != 0x8087) &&  // Intel
       (info->dev_vendor != 0x0451) &&
       (info->dev_vendor != 0x0502) &&
       (info->dev_vendor != 0x0fce) &&  // Sony Ericsson
       (info->dev_vendor != 0x05c6) &&  // Qualcomm
       (info->dev_vendor != 0x22b8) &&  // Motorola
       (info->dev_vendor != 0x0955) &&  // Nvidia
       (info->dev_vendor != 0x413c) &&  // DELL
       (info->dev_vendor != 0x2314) &&  // INQ Mobile
       (info->dev_vendor != 0x0b05) &&  // Asus
       (info->dev_vendor != 0x0bb4))    // HTC
            return -1;
    if(info->ifc_class != 0xff) return -1;
    if(info->ifc_subclass != 0x42) return -1;
    if(info->ifc_protocol != 0x03) return -1;
    // require matching serial number or device path if requested
    // at the command line with the -s option.
    if (local_serial && (strcmp(local_serial, info->serial_number) != 0 &&
                   strcmp(local_serial, info->device_path) != 0)) return -1;
    return 0;
}

int match_fastboot(usb_ifc_info *info)
{
    return match_fastboot_with_serial(info, serial);
}

static int64_t file_size(int fd)
{
    struct stat st;
    int ret;

    ret = fstat(fd, &st);

    return ret ? -1 : st.st_size;
}


static void *load_fd(int fd, unsigned *_sz)
{
    char *data;
    int sz;
    int errno_tmp;

    data = 0;

    sz = file_size(fd);
    if (sz < 0) {
        goto oops;
    }

    data = (char*) malloc(sz);
    if(data == 0) goto oops;

    if(read(fd, data, sz) != sz) goto oops;
    close(fd);

    if(_sz) *_sz = sz;
    return data;

oops:
    errno_tmp = errno;
    close(fd);
    if(data != 0) free(data);
    errno = errno_tmp;
    return 0;
}


static void *load_file(const char *fn, unsigned *_sz)
{
    int fd;

    fd = open(fn, O_RDONLY | O_BINARY);
    if(fd < 0) return 0;

    return load_fd(fd, _sz);
}

void *load_bootable_image(const char *kernel, const char *ramdisk,
                          const char *secondstage, unsigned *sz,
                          const char *cmdline)
{
    void *kdata = 0, *rdata = 0, *sdata = 0;
    unsigned ksize = 0, rsize = 0, ssize = 0;
    void *bdata;
    unsigned bsize;

    if(kernel == 0) {
        fprintf(stderr, "no image specified\n");
        return 0;
    }

    kdata = load_file(kernel, &ksize);
    if(kdata == 0) {
        fprintf(stderr, "cannot load '%s': %s\n", kernel, strerror(errno));
        return 0;
    }

        /* is this actually a boot image? */
    if(!memcmp(kdata, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
        if(cmdline) bootimg_set_cmdline((boot_img_hdr*) kdata, cmdline);

        if(ramdisk) {
            fprintf(stderr, "cannot boot a boot.img *and* ramdisk\n");
            return 0;
        }

        *sz = ksize;
        return kdata;
    }

    if(ramdisk) {
        rdata = load_file(ramdisk, &rsize);
        if(rdata == 0) {
            fprintf(stderr,"cannot load '%s': %s\n", ramdisk, strerror(errno));
            return  0;
        }
    }

    if (secondstage) {
        sdata = load_file(secondstage, &ssize);
        if(sdata == 0) {
            fprintf(stderr,"cannot load '%s': %s\n", secondstage, strerror(errno));
            return  0;
        }
    }

    fprintf(stderr,"creating boot image...\n");
    bdata = mkbootimg(kdata, ksize, kernel_offset,
                      rdata, rsize, ramdisk_offset,
                      sdata, ssize, second_offset,
                      page_size, base_addr, tags_offset, &bsize);
    if(bdata == 0) {
        fprintf(stderr,"failed to create boot.img\n");
        return 0;
    }
    if(cmdline) bootimg_set_cmdline((boot_img_hdr*) bdata, cmdline);
    fprintf(stderr,"creating boot image - %d bytes\n", bsize);
    *sz = bsize;

    if (kdata)
	    free(kdata);
    if (rdata)
	    free(rdata);
    if (sdata)
	    free(sdata);    

    return bdata;
}


usb_handle *open_device(void)
{
	static usb_handle *usb = 0;
	int announce = 1;
		
	if(usb) return usb;
		
	for(;;) {
		usb = usb_open(match_fastboot);
		if(usb) return usb;
		if(announce) {
			announce = 0;
			fprintf(stderr,"< waiting for device >\n");
		}
		sleep(3);
	}
}
	
int main(int argc, char **argv)
{
	static usb_handle *usb = 0;
	void *data;
	unsigned sz;

	if (argc < 2) {
		printf("Quick and dirty fastboot server\n");
		printf("Usage: fastbootd kernel [initrd] [secondstage] \n");
		printf("Hacked from google's fastboot sources in a few hours in 2015 \n");
		printf("by Necromant <spam at ncrmnt.org>  \n");
		exit(1);
	}

	usb = open_device();
	
	char *kname = NULL;
	char *sname = NULL;
	char *rname = NULL;

	if (argc > 1) 
		kname = argv[1];
	
	if (argc > 2) 
		rname = argv[2];
	
	if (argc > 3) 
		sname = argv[3];
		
	data = load_bootable_image(kname, rname, sname, &sz, NULL);
	if (data == 0) return 1;
	fb_queue_download("boot.img", data, sz);
	fb_queue_command("boot", "booting");
	if (fb_queue_is_empty())
			return 0;
	
	int ret = fb_execute_queue(usb);

	/*   
	 *   apparently folks at google didn't take care to 
	 *   free memory in places where they should. 
	 *   Digging deep into their hackery is not for the faint-hearted,
	 *   so I decided to just restart the process itself.  
	 */
	
	execl(argv[0], argv[0], kname, rname, sname, (char *) NULL);
}

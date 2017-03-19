ramdisk: ramdisk.c
	gcc -Wall ramdisk.c `pkg-config fuse --cflags --libs` -o ramdisk

clean:
	rm -rf ramdisk

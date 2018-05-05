all: sffs

sffs: sffs.c
	gcc -D_FILE_OFFSET_BITS=64 -o $@ $^ -lfuse

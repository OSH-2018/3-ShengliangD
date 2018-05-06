test: sffs
	./sffs tmp & sleep 0.2; cd tmp; bash; cd ..; sudo umount -f tmp

debug: sffs
	./sffs -d tmp & sleep 0.2; cd tmp; bash; cd ..; sudo umount -f tmp

sffs: sffs.c
	gcc -D_FILE_OFFSET_BITS=64 -o $@ $^ -lfuse -g

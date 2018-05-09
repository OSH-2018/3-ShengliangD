try: sffs
	mkdir -p tmp
	./sffs tmp & sleep 0.2 && cd tmp && zsh && cd .. && sudo umount -f tmp
	rmdir tmp

debug: sffs
	mkdir -p tmp
	./sffs -d tmp & sleep 0.2 && cd tmp && zsh && cd .. && sudo umount -f tmp
	rmdir tmp

sffs: sffs.o sffs_blocks.o
	gcc -o $@ $^ -lfuse

%.o: %.c
	gcc -D_FILE_OFFSET_BITS=64 -c -o $@ $^

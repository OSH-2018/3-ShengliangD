try: sffs
	mkdir -p tmp
	./sffs tmp & sleep 0.2 && cd tmp && zsh && cd .. && sudo umount -f tmp
	rmdir tmp

debug: sffs
	mkdir -p tmp
	./sffs -d tmp & sleep 0.2 && cd tmp && zsh && cd .. && sudo umount -f tmp
	rmdir tmp

test: sffs
	./test.sh

sffs: sffs.c
	gcc -D_FILE_OFFSET_BITS=64 -o $@ $^ -lfuse -O3

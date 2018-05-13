NAME=sffs
CC=gcc
CC_FLAGS=-D_FILE_OFFSET_BITS=64
LD_LIBS=-lfuse
TMP_DIR=tmp
SHELL=zsh

all: ${NAME}

debug: ${NAME}
	mkdir -p ${TMP_DIR}
	./sffs -d ${TMP_DIR} & sleep 0.2 && cd ${TMP_DIR} && ${SHELL} && cd .. && sudo umount -f ${TMP_DIR}
	rmdir ${TMP_DIR}

${NAME}: ${NAME}.o ${NAME}_blocks.o
	${CC} ${LD_LIBS} -o $@ $^

%.o: %.c
	${CC} ${CC_FLAGS} -c -o $@ $^

clean:
	rm -rf *.o ${NAME}

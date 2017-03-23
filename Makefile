CC=gcc
CFLAGS= -std=gnu99 -Wall
LDLIBS = -lpthread -lm -lrt
TARGET=main
FILES=main.o
${TARGET}: ${FILES}
	${CC} -o ${TARGET} ${FILES} ${LDLIBS}
${FILES}: ${TARGET}.c
	${CC} -o ${FILES} -c ${TARGET}.c ${CFLAGS}
.PHONY: clean
clean:
	-rm -f ${FILES} ${TARGET}

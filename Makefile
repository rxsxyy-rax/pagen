CC = cc

PREFIX = /usr/local

CFLAGS = -std=c99 -Oz -Wall -Wextra

SRC = pagen.c
OUT = pagen

all: ${SRC}
	${CC} ${SRC} ${CFLAGS} -o ${OUT}

clean:
	rm -rf ${OUT}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${OUT} ${DESTDIR}${PREFIX}/bin/
	chmod 755 ${DESTDIR}${PREFIX}/bin/${OUT}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${OUT}

.PHONY: all clean install uninstall

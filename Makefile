PREFIX  := /usr/local
CC      := cc
CFLAGS  := -pedantic -Wall -Wno-deprecated-declarations -Os
LDFLAGS := -lX11 -lpthread

LDFLAGS += -L/usr/X11R6/lib -I/usr/X11R6/include

all: options sblocks

options:
	@echo sblocks build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"

sblocks: sblocks.c config.h
	${CC} -o sblocks sblocks.c ${CFLAGS} ${LDFLAGS}

blocks.h:
	cp blocks.def.h $@

clean:
	rm -f *.o *.gch sblocks

install: sblocks
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f sblocks ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/sblocks

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/sblocks

.PHONY: all options clean install uninstall

PREFIX  := /usr/local
CC      := cc
CFLAGS  := -pedantic -Wall -Wno-deprecated-declarations -Os
LDFLAGS := -lX11 -lpthread

LDFLAGS += -L/usr/X11R6/lib -I/usr/X11R6/include

all: options ipcbar

options:
	@echo ipcbar build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"

ipcbar: ipcbar.c config.h
	${CC} -o ipcbar ipcbar.c ${CFLAGS} ${LDFLAGS}

blocks.h:
	cp blocks.def.h $@

clean:
	rm -f *.o *.gch ipcbar

install: ipcbar
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ipcbar ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/ipcbar

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/ipcbar

.PHONY: all options clean install uninstall

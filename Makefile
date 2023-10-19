APP    = teapot

CC     = gcc
CFLAGS = 
CLIBS  = 
RM     = rm -f

PREFIX = ${HOME}/usr/local

default: tomato

tomato: teapot.c config.h libcsv.a
	$(CC) $(CFLAG) -o ${APP} teapot.c libcsv.a -I./include $(CLIBS)

config.h:
	cp config.def.h $@

libcsv.a:
	gcc external/libcsv/libcsv.c -c -o libcsv.o
	ar -rc libcsv.a libcsv.o
	ar -s libcsv.a
	cp ./external/libcsv/csv.h ./include

clean:
	$(RM) ${APP}

install: tomato
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${APP} ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/${APP}

uninstall: 
	rm -f $(DESTDIR)$(PREFIX)/bin/${APP}


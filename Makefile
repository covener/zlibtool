#
# $Log$
# Revision 1.5  2000/08/18 14:26:33  trawick
# Add a "clean" target.
#
# Revision 1.4  2000/08/14 14:59:47  trawick
# Add initial support for building Apache 2.0 dsos.
#
# Known problems with this level of code:
#
# 1) libtoolexe.c code needs to be split up; too darn big
# 2) dsos aren't linked until "make install", which is too late
#
# Revision 1.3  2000/06/30 14:26:14  trawick
# Install config.sub and config.guess.
# Make sure target directories exist.
#
# Revision 1.2  2000/06/30 13:39:27  trawick
# Add install target; fix CFLAGS; don't build libtoolize (not .c anymore).
#
# Revision 1.1  2000/06/29 15:26:58  trawick
# initial check-in
#
#

CFLAGS=-D_ALL_SOURCE -g
LFLAGS=-g

all: libtoolexe
# libtoolize

install: libtoolexe libtool.m4 libtool libtoolize ltconfig config.guess config.sub shlibtool
	chmod +x ./check_libtool_prefix
	./check_libtool_prefix
	mkdir -p $(LIBTOOL_PREFIX)/bin
	cp -p libtoolexe $(LIBTOOL_PREFIX)/bin
	cp -p libtool $(LIBTOOL_PREFIX)/bin
	chmod +x $(LIBTOOL_PREFIX)/bin/libtool
	cp -p shlibtool $(LIBTOOL_PREFIX)/bin
	chmod +x $(LIBTOOL_PREFIX)/bin/shlibtool
	cp -p libtoolize $(LIBTOOL_PREFIX)/bin
	chmod +x $(LIBTOOL_PREFIX)/bin/libtoolize
	cp -p ltconfig $(LIBTOOL_PREFIX)/bin
	chmod +x $(LIBTOOL_PREFIX)/bin/ltconfig
	mkdir -p $(LIBTOOL_PREFIX)/share/aclocal
	cp -p libtool.m4 $(LIBTOOL_PREFIX)/share/aclocal
	mkdir -p $(LIBTOOL_PREFIX)/share/libtool
	cp -p config.guess $(LIBTOOL_PREFIX)/share/libtool/config.guess
	chmod +x $(LIBTOOL_PREFIX)/share/libtool/config.guess
	cp -p config.sub $(LIBTOOL_PREFIX)/share/libtool/config.sub
	chmod +x $(LIBTOOL_PREFIX)/share/libtool/config.sub

clean:
	rm -f *.o libtoolexe

libtoolexe: libtoolexe.o
	$(CC) $(LFLAGS) -o libtoolexe libtoolexe.o

libtoolexe.o: libtoolexe.c
	$(CC) $(CFLAGS) -c libtoolexe.c


#
# $Log$
# Revision 1.2  2000/06/30 13:39:27  trawick
# Add install target; fix CFLAGS; don't build libtoolize (not .c anymore).
#
# Revision 1.1  2000/06/29 15:26:58  trawick
# initial check-in
#
#

CFLAGS=-D_ALL_SOURCE

all: libtoolexe
# libtoolize

install: libtoolexe libtool.m4 libtool libtoolize ltconfig config.guess config.sub
	chmod +x ./check_libtool_prefix
	./check_libtool_prefix
	mkdir -p $(LIBTOOL_PREFIX)/bin
	cp -p libtoolexe $(LIBTOOL_PREFIX)/bin
	cp -p libtool $(LIBTOOL_PREFIX)/bin
	chmod +x $(LIBTOOL_PREFIX)/bin/libtool
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

libtoolexe: libtoolexe.o
	c89 -o libtoolexe libtoolexe.o

libtoolexe.o: libtoolexe.c
	c89 $(CFLAGS) -c libtoolexe.c


#
# $Log$
# Revision 1.1  2000/06/29 15:26:58  trawick
# initial check-in
#
#

CFLAGS=-D_ALL_SOURCE

all: libtoolexe
# libtoolize

install: libtoolexe libtool.m4 libtool libtoolize ltconfig
	chmod +x ./check_libtool_prefix
	./check_libtool_prefix
	cp -p libtoolexe $(LIBTOOL_PREFIX)/bin
	cp -p libtool $(LIBTOOL_PREFIX)/bin
	chmod +x $(LIBTOOL_PREFIX)/bin/libtool
	cp -p libtoolize $(LIBTOOL_PREFIX)/bin
	chmod +x $(LIBTOOL_PREFIX)/bin/libtoolize
	cp -p ltconfig $(LIBTOOL_PREFIX)/bin
	chmod +x $(LIBTOOL_PREFIX)/bin/ltconfig
	cp -p libtool.m4 $(LIBTOOL_PREFIX)/share/aclocal

libtoolexe: libtoolexe.o
	c89 -o libtoolexe libtoolexe.o

libtoolexe.o: libtoolexe.c
	c89 $(CFLAGS) -c libtoolexe.c


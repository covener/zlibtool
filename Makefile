#
# $Log$
#
all: libtoolexe
# libtoolize

libtoolexe: libtoolexe.o
	c89 -o libtoolexe libtoolexe.o

libtoolexe.o:
	c89 -c libtoolexe.c

libtoolize: libtool.c
	c89 -o libtoolize libtoolize.c


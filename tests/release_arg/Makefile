LIBTOOL=../../libtool
CFLAGS="-D_UNIX03_SOURCE -U_NO_PROTO -Wc,XPLINK,lp64,dll,expo -Wl,XPLINK,lp64"

all: libhello.la main install

libhello.la: hello.lo
	$(LIBTOOL) --mode=link $(CC) $(CFLAGS) -release 2 -o libhello.la hello.lo

hello.lo: hello.c
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) -o hello.lo -c hello.c 

main: main.o
	$(LIBTOOL) --mode=link $(CC) $(CFLAGS) -o main main.o
main.o: main.c
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) -c main.c

install: libhello.la
	mkdir -p built
	$(LIBTOOL) --mode=install ../install.sh -c libhello.la built/


clean:
	rm -f main *.o *.a *.lo *.la *.so *.x
	rm -rf .libs/
	rm -rf built

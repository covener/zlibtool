# ====================================================================
# The Apache Software License, Version 1.1
#
# Copyright (c) 2000-2002 The Apache Software Foundation.  All rights
# reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
#
# 3. The end-user documentation included with the redistribution,
#    if any, must include the following acknowledgment:
#       "This product includes software developed by the
#        Apache Software Foundation (http://www.apache.org/)."
#    Alternately, this acknowledgment may appear in the software itself,
#    if and wherever such third-party acknowledgments normally appear.
#
# 4. The names "Apache" and "Apache Software Foundation" must
#    not be used to endorse or promote products derived from this
#    software without prior written permission. For written
#    permission, please contact apache@apache.org.
#
# 5. Products derived from this software may not be called "Apache",
#    nor may "Apache" appear in their name, without prior written
#    permission of the Apache Software Foundation.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
# ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
# ====================================================================
#
# This software consists of voluntary contributions made by many
# individuals on behalf of the Apache Software Foundation.  For more
# information on the Apache Software Foundation, please see
# <http://www.apache.org/>.

CFLAGS=-D_ALL_SOURCE -g
LFLAGS=-g

all: libtoolexe

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
	cp -p libtool_printpath $(LIBTOOL_PREFIX)/bin
	chmod +x $(LIBTOOL_PREFIX)/bin/libtool_printpath
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


#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#
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


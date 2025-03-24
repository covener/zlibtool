# libtool for building Apache on z/OS


## Installation of this libtool on z/OS:

2) make
3) export LIBTOOL_PREFIX=xxx     (e.g., /usr/local)
4) make install

This is a substitute for libtool on zOS that acts just enough like libtool to compile/link simple things
and and some special knowledge of HTTPD/APR.   Some of the zOS quirks are that `-l` does static linking
and that the order of object files vs options is very strict.

## Building Apache on z/OS:

Please see http://web.archive.org/web/20130607014924/http://people.apache.org/~trawick/apache-2-on-os390.html
for historical information. Things have changed a bit since the original port and the info here and below should
be moved to cwiki.

- The Open Mainframe Project provides ascii-forward ports of bash, git, autoconf, and m4 (but not yet a usable libtool)
   <https://github.com/zopencommunity>
- There is an `openxlc` compiler that is more friendly to porting, but not so much when the projects are explicitly 
   ported to EBCDIC and the native xlc compiler.
-  You will probably want both your interactiv shell and CONFIG_SHELL envvar to point to an ascii-aware bash
   and you should have the basic zopen auto-conversion in your environment.
- A reasonable CLFAGS is `export CFLAGS="-Wc,-qcpluscmt -Wc,-qlanglvl=extc99 -Wc,-qhaltonmsg=CCN3280 -Wc,-qhaltonmsg=CCN3296 -Wc,XPLINK,lp64,dll,expo -Wl,XPLINK,lp64 -O2 -Wc,-qstrict`
- The `CC=cc` frontend is what's usually used for Apache. To use xlc or any other variant, apr_hints.m4 has to turn on many flags that say "show me all the OSS symbols".

Bugs:

  - Eats option after *.la

## TOOD

1. Use the technique in https://community.ibm.com/community/user/ibmz-and-linuxone/blogs/kai-nacke/2025/02/20/building-shared-libraries-with-ibm-openxl?communityKey=5805da79-8284-4015-97fb-5a19f6480452
   where .x files are added to .a files to allow simply -lfoo to work. Author says it works with xlc too.
   - https://www.ibm.com/docs/en/zos/3.1.0?topic=references-autocall-archive-libraries
2. remove BEOS

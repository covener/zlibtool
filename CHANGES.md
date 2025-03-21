## Unreleased changes

- add support for `-release X` as used by APU stubs.

## Changes with zlibtool 1.5.0

- Avoid -Wl,DLL with the compiler is ibm-clang

## Changes with zlibtool 1.4.0-fix3

- Use tar -S to preserve file tags and encodings.

## Changes with zlibtool 1.4.0-fix2

- When linking a shared library foo/bar/baz.la, use .libs/baz.a not .libs/foo/bar/baz.a

## Changes in zlibtool 1.4.0-fix1

- Bump version to 1.4.0 to differentiate with IHS 2.2 zlibtool
- Backport "use relative libtoolexe" from IHS build copy of `libtool`


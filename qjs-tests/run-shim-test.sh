#!/bin/bash
NS_WORKSPACE=${NS_WORKSPACE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
NS_SRC=${NS_SRC:-$NS_WORKSPACE/netsurf}
# Build and run the duk_compat unit test with ASan/UBSan.
set -e
S=$NS_SRC
E=$S/content/handlers/javascript/quickjs/engine
B=${1:-$NS_WORKSPACE/build-qjs-tests}
mkdir -p "$B"
for f in quickjs libregexp libunicode dtoa; do
	[ "$B/$f.o" -nt "$E/$f.c" ] || gcc -std=gnu11 -O1 -g -w -fsanitize=address,undefined -c "$E/$f.c" -o "$B/$f.o"
done
gcc -std=c99 -D_POSIX_C_SOURCE=200809L -g -O1 -W -Wall -Wno-unused-parameter \
	-Wmissing-prototypes -Wstrict-prototypes -Werror -fsanitize=address,undefined \
	-I$S -I$S/include -I$S/content/handlers \
	-c $S/content/handlers/javascript/quickjs/duk_compat.c -o "$B/duk_compat.o"
gcc -std=c99 -g -O1 -w -fsanitize=address,undefined -I$S -I$S/include -I$S/content/handlers \
	"$(dirname "$0")/test_duk_compat.c" "$B"/duk_compat.o "$B"/quickjs.o "$B"/libregexp.o \
	"$B"/libunicode.o "$B"/dtoa.o -lm -o "$B/test_duk_compat"
"$B/test_duk_compat"

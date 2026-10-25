#!/bin/bash
NS_WORKSPACE=${NS_WORKSPACE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
NS_SRC=${NS_SRC:-$NS_WORKSPACE/netsurf}
# Build the duktape, QuickJS and QuickJS sanitizer monkey frontends and
# the QuickJS GTK frontend.
set -e
cd "$NS_WORKSPACE"
unset HOST
export TARGET_WORKSPACE=$PWD
source ./env.sh > /dev/null
cd netsurf
make -j16 TARGET=monkey OBJROOT=$TARGET_WORKSPACE/build-monkey EXETARGET=$TARGET_WORKSPACE/nsmonkey > $TARGET_WORKSPACE/build-monkey.log 2>&1 || { echo "duktape build failed"; grep -E 'error' $TARGET_WORKSPACE/build-monkey.log | head; exit 1; }
make -j16 TARGET=monkey NETSURF_USE_QUICKJS=YES NETSURF_USE_NSSVG=YES OBJROOT=$TARGET_WORKSPACE/build-monkey-qjs EXETARGET=$TARGET_WORKSPACE/nsmonkey-qjs > $TARGET_WORKSPACE/build-monkey-qjs.log 2>&1 || { echo "quickjs build failed"; grep -E 'error' $TARGET_WORKSPACE/build-monkey-qjs.log | head; exit 1; }
CFLAGS="-Wno-error=stringop-truncation" make -j16 TARGET=monkey NETSURF_USE_QUICKJS=YES NETSURF_USE_NSSVG=YES NETSURF_USE_SANITIZER=YES NETSURF_RECOVER_SANITIZERS=NO OBJROOT=$TARGET_WORKSPACE/build-monkey-qjs-asan EXETARGET=$TARGET_WORKSPACE/nsmonkey-qjs-asan > $TARGET_WORKSPACE/build-asan.log 2>&1 || { echo "sanitizer build failed"; grep -E 'error' $TARGET_WORKSPACE/build-asan.log | head; exit 1; }
make -j16 TARGET=gtk NETSURF_GTK_MAJOR=3 NETSURF_USE_QUICKJS=YES OBJROOT=$TARGET_WORKSPACE/build-gtk3-qjs EXETARGET=$TARGET_WORKSPACE/nsgtk3-qjs > $TARGET_WORKSPACE/build-gtk3-qjs.log 2>&1 || { echo "gtk quickjs build failed"; grep -E 'error' $TARGET_WORKSPACE/build-gtk3-qjs.log | head; exit 1; }
echo "all builds ok"

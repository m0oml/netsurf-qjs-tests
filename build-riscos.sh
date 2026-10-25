#!/bin/bash
NS_WORKSPACE=${NS_WORKSPACE:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}
NS_SRC=${NS_SRC:-$NS_WORKSPACE/netsurf}
# Cross-build the RISC OS frontend with QuickJS from a copy of src/,
# because the RISC OS makefile writes the application into its source tree.
#
#   build-riscos.sh [arm-unknown-riscos|arm-riscos-gnueabi] [make args...]

cd "$NS_WORKSPACE"
export HOST=${1:-arm-unknown-riscos} TARGET_WORKSPACE=$PWD
shift
source ./env.sh > /dev/null
set -e
SRC=riscos-src-$HOST
rm -rf $SRC && mkdir $SRC
tar -C netsurf/ --exclude=.git -cf - . | tar -C $SRC -xf -
cd $SRC
make -j16 TARGET=riscos GCCSDK_INSTALL_ENV=/opt/netsurf/$HOST/env GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/$HOST/cross/bin NETSURF_USE_QUICKJS=YES NETSURF_USE_DUKTAPE=NO OBJROOT=$TARGET_WORKSPACE/build-riscos-qjs-$HOST "$@"

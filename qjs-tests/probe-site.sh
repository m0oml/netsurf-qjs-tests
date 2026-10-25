#!/bin/bash
NS_WORKSPACE=${NS_WORKSPACE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
NS_SRC=${NS_SRC:-$NS_WORKSPACE/netsurf}
# Load a site in the QuickJS monkey build, then run a script in it and show its console output.
#
# usage: probe-site.sh <url> <script file> [load seconds]
set -u
URL=$1
SCRIPT=$(tr '\n' ' ' < "$2")
LOAD=${3:-20}
BIN=$NS_WORKSPACE/nsmonkey-qjs
export NETSURFRES=$NS_SRC/frontends/monkey/res
(printf 'WINDOW NEW %s\n' "$URL"; sleep "$LOAD"; printf 'WINDOW EXEC 0 JS %s\n' "$SCRIPT"; sleep 5; echo QUIT) |
	timeout $((LOAD + 40)) "$BIN" --enable_javascript=1 2>&1 |
	grep -aE 'CONSOLE_LOG|UPDATE_EXTENT|WINDOW TITLE' | sed -E 's/WINDOW CONSOLE_LOG WIN [0-9]+ SOURCE [a-z-]+ [A-Z-]+ //'

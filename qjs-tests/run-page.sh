#!/bin/bash
NS_WORKSPACE=${NS_WORKSPACE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
NS_SRC=${NS_SRC:-$NS_WORKSPACE/netsurf}
# Load a test page in a monkey build and print its console PASS/FAIL lines.
#
# usage: run-page.sh <monkey binary> <page.html or URL> [seconds]
#
# Pages report results with console.log("PASS name") / ("FAIL name") and
# finish with console.log("DONE").  Uncaught JS errors are also shown.
set -u
BIN=$1
case $2 in
http:*|https:*) URL=$2 ;;
*) URL="file://$(realpath "$2")" ;;
esac
WAIT=${3:-3}
export NETSURFRES=$NS_SRC/frontends/monkey/res
OUT=$(mktemp)
(printf 'WINDOW NEW %s\n' "$URL"; sleep "$WAIT"; echo QUIT) |
	timeout $((WAIT + 20)) "$BIN" --enable_javascript=1 > "$OUT" 2>&1
RC=$?
grep -E 'CONSOLE_LOG' "$OUT" | sed -E 's/.*CONSOLE_LOG WIN [0-9]+ SOURCE [a-z-]+ [A-Z-]+ [A-Z]+ //'
grep -E 'jserrors|AddressSanitizer|runtime error|Assertion' "$OUT" | cut -c1-300
echo "exit=$RC pass=$(grep -c 'LOG PASS' "$OUT") fail=$(grep -c 'LOG FAIL' "$OUT") done=$(grep -c 'LOG DONE' "$OUT")"
rm -f "$OUT"

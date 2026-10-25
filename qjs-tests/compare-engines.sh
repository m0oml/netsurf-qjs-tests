#!/bin/bash
NS_WORKSPACE=${NS_WORKSPACE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
NS_SRC=${NS_SRC:-$NS_WORKSPACE/netsurf}
# Run every page of a directory through two monkey builds and compare
# console output, uncaught JS errors and sanitizer reports.
#
# usage: compare-engines.sh <reference binary> <test binary> [page dir] [seconds]
set -u
REF=$1
TEST=$2
DIR=${3:-$NS_SRC/test/js}
WAIT=${4:-1.5}
OUT=$(mktemp -d)
export NETSURFRES=$NS_SRC/frontends/monkey/res
export ASAN_OPTIONS=detect_leaks=0
regress=0
for f in "$DIR"/*.html; do
	n=$(basename "$f" .html)
	for b in "$REF" "$TEST"; do
		k=$(basename "$b")
		(printf 'WINDOW NEW file://%s\n' "$(realpath "$f")"; sleep "$WAIT"; echo QUIT) |
			timeout 30 "$b" --enable_javascript=1 > "$OUT/$n.$k.raw" 2>&1
		echo "exit=$?" >> "$OUT/$n.$k.raw"
		grep -E 'CONSOLE_LOG|jserrors|AddressSanitizer|runtime error|^exit=' "$OUT/$n.$k.raw" |
			grep -v 'LOG Frame' |
			sed -E 's/^\([0-9.]+\) //; s/[a-z_/]+\.c:[0-9]+ [a-z_]+: //' > "$OUT/$n.$k"
	done
	r="$OUT/$n.$(basename "$REF")"
	t="$OUT/$n.$(basename "$TEST")"
	re=$(grep -c jserrors "$r"); te=$(grep -c jserrors "$t")
	bad=$(grep -cE 'Sanitizer|runtime error' "$t")
	tx=$(grep '^exit=' "$t")
	if cmp -s <(grep -v jserrors "$r") <(grep -v jserrors "$t"); then same=same; else same=DIFF; fi
	flag=""
	if [ "$te" -gt "$re" ] || [ "$bad" -gt 0 ] || [ "$same" = DIFF ] || [ "$tx" != "exit=0" ]; then
		flag="  <<<"; regress=$((regress + 1))
	fi
	printf '%-36s ref_err=%s test_err=%s console=%s %s%s\n' "$n" "$re" "$te" "$same" "$tx" "$flag"
done
echo "pages needing attention: $regress (raw output in $OUT)"

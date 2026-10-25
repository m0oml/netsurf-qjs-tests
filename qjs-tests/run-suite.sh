#!/bin/bash
NS_WORKSPACE=${NS_WORKSPACE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
NS_SRC=${NS_SRC:-$NS_WORKSPACE/netsurf}
# Run every PASS/FAIL test page in qjs-tests/pages through a monkey build.
#
# usage: run-suite.sh [monkey binary]
#
# Pages which report results end with console.log("DONE"); others (probes,
# benchmarks) are skipped.  Exits non-zero if any check fails, a page does
# not finish, or a sanitizer reports a problem.  Pages which mention
# net-server.py are loaded from that server, if it is running.
set -u
BIN=${1:-$NS_WORKSPACE/nsmonkey-qjs-asan}
DIR=$NS_WORKSPACE/qjs-tests/pages
NETSERVER=http://127.0.0.1:8701
export ASAN_OPTIONS=detect_leaks=0
total_pass=0
total_fail=0
bad=0
for page in "$DIR"/*.html; do
	grep -q 'console.log("DONE")' "$page" || continue
	grep -q 'PASS' "$page" || continue
	target=$page
	wait=2
	if grep -q 'net-server.py' "$page"; then
		# needs the test server, see net-server.py
		if ! curl -s -o /dev/null "$NETSERVER/api/json"; then
			printf '%-24s skipped (net-server.py not running)\n' "$(basename "$page" .html)"
			continue
		fi
		target="$NETSERVER/$(basename "$page")"
		wait=25
	fi
	out=$("$(dirname "$0")/run-page.sh" "$BIN" "$target" $wait)
	summary=$(echo "$out" | tail -1)
	pass=$(echo "$summary" | sed -E 's/.*pass=([0-9]+).*/\1/')
	fail=$(echo "$summary" | sed -E 's/.*fail=([0-9]+).*/\1/')
	done=$(echo "$summary" | sed -E 's/.*done=([0-9]+).*/\1/')
	problems=$(echo "$out" | grep -E '^FAIL|Sanitizer|runtime error|Assertion')
	total_pass=$((total_pass + pass))
	total_fail=$((total_fail + fail))
	status=ok
	if [ "$fail" != 0 ] || [ "$done" != 1 ] || [ -n "$problems" ] || ! echo "$summary" | grep -q 'exit=0'; then
		status=BAD
		bad=$((bad + 1))
	fi
	printf '%-24s %s pass=%s fail=%s\n' "$(basename "$page" .html)" "$status" "$pass" "$fail"
	[ -n "$problems" ] && echo "$problems" | sed 's/^/    /'
done
echo "total pass=$total_pass fail=$total_fail pages with problems=$bad"
[ "$bad" = 0 ]

#!/bin/bash
NS_WORKSPACE=${NS_WORKSPACE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
NS_SRC=${NS_SRC:-$NS_WORKSPACE/netsurf}
# Run the test suite in the RISC OS build under RPCEmu.
#
# usage: run-suite-riscos.sh [max seconds]
#
# Needs net-server.py running on the libvirt bridge, where the emulator
# can reach it:   net-server.py --host=192.168.122.1
# and the build copied to HostFS:$.NSQuickJS.!NetSurf (with the
# SharedULib and ARMEABISupport modules beside it).
#
# A temporary boot task starts NetSurf at /api/suite; each page reports
# its console.log lines back to the server, which this script collects.
# The boot task is removed afterwards.
set -u
MAX=${1:-1800}
SERVER=http://${NS_SERVER_HOST:-192.168.122.1}:8701
HOSTFS=${HOSTFS:-/opt/rpcemu-direct/hostfs}
TASK="$HOSTFS/!Boot/Choices/Boot/Tasks/ZNSQJSTest,feb"

if ! curl -s -o /dev/null "$SERVER/api/json"; then
	echo "net-server.py is not running on $SERVER" >&2
	exit 2
fi
curl -s -o /dev/null "$SERVER/api/reports?clear=1"

cat > "$TASK" <<EOF
RMEnsure SharedUnixLibrary 1.16 RMLoad HostFS:\$.NSQuickJS.SharedULib
RMEnsure ARMEABISupport 1.04 RMLoad HostFS:\$.NSQuickJS.ARMEABISupport
WimpTask /HostFS:\$.NSQuickJS.!NetSurf --enable_javascript=1 -url $SERVER/api/suite
EOF

pkill -TERM -x rpcemu-recompil && sleep 3
(cd /tmp && setsid rpcemu-direct > /dev/null 2>&1 &)

start=$(date +%s)
while ! curl -s "$SERVER/api/reports" | grep -q 'SUITE DONE'; do
	if [ $(( $(date +%s) - start )) -gt "$MAX" ]; then
		echo "suite did not finish within $MAX seconds"
		break
	fi
	sleep 10
done
echo "elapsed $(( $(date +%s) - start ))s"

rm -f "$TASK"

curl -s "$SERVER/api/reports" | python3 -I -c '
import json, sys
reports = json.load(sys.stdin)
pages = {}
for r in reports:
    if r.get("page"):
        pages.setdefault(r["page"], []).append(r)
total_pass = total_fail = bad = 0
for page, lines in pages.items():
    lines.sort(key=lambda r: r["seq"])
    msgs = [r["msg"] for r in lines]
    p = sum(m.startswith("PASS") for m in msgs)
    f = sum(m.startswith("FAIL") for m in msgs)
    done = "DONE" in msgs
    problems = [m for m in msgs if m.startswith(("FAIL", "ERROR", "TIMEOUT"))]
    total_pass += p
    total_fail += f
    status = "ok" if done and not problems else "BAD"
    bad += status == "BAD"
    print("%-24s %s pass=%d fail=%d" % (page[:-5], status, p, f))
    for m in problems:
        print("    " + m[:200])
print("total pass=%d fail=%d pages with problems=%d" % (total_pass, total_fail, bad))
'

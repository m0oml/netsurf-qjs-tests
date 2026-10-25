#!/bin/bash
NS_WORKSPACE=${NS_WORKSPACE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
NS_SRC=${NS_SRC:-$NS_WORKSPACE/netsurf}
# Open a page in the QuickJS GTK build, wait, screenshot its window.
#
# usage: gtk-shot.sh <page.html> <out.png> [seconds]
set -u
case $1 in http:*|https:*) URL=$1 ;; *) URL="file://$(realpath "$1")" ;; esac
PNG=$2
WAIT=${3:-5}
cd "$NS_WORKSPACE"
LOG="${PNG%.png}.log"
(NETSURFRES=$NS_SRC/frontends/gtk/res ./nsgtk3-qjs --enable_javascript=1 "$URL" > "$LOG" 2>&1 &)
sleep "$WAIT"
G=$(hyprctl clients -j | python3 -I -c "import json,sys; c=[c for c in json.load(sys.stdin) if c['class']=='nsgtk3-qjs']; print('%d,%d %dx%d' % (c[0]['at'][0], c[0]['at'][1], c[0]['size'][0], c[0]['size'][1]) if c else 'none')")
echo "geom=$G"
[ "$G" != none ] && grim -g "$G" "$PNG"
pkill -x nsgtk3-qjs
grep -iE 'assert|Segment|jserrors' "$LOG" | head

#!/usr/bin/env python3
"""HTTP test server for script network requests.

Serves qjs-tests/pages statically and test endpoints under /api/ on two
ports on 127.0.0.1, so pages can make same origin and cross origin
requests:

    net-server.py [--host=addr] [port] [port2]
                                      (defaults 127.0.0.1, 8701 and 8702)

Endpoints (query parameters in brackets):
  /api/echo            JSON of method, path, query, headers and body
  /api/status/N        status N with a short text body
  /api/redirect        redirect [to, code]
  /api/loop            redirects to itself
  /api/setcookie       sets cookie [name, value]
  /api/cookies         JSON of the cookies sent
  /api/cors            echo with CORS headers [acao, acac, aceh, acam, acah,
                       maxage]; OPTIONS answers preflights with the same
  /api/latin1          ISO-8859-1 text
  /api/slow            responds after [ms] milliseconds
  /api/big             [n] bytes of text
  /api/json            a JSON document
  /api/log             JSON list of requests seen (method, path, origin)

Running the suite in a browser without a console (the RISC OS build):
  /api/suite           starts at the first test page; each page is served
                       with ?ro=N, which adds a script that posts its
                       console.log lines to /api/report and moves on to
                       /api/next?i=N after DONE or a timeout
  /api/reports         JSON list of the lines reported [clear]
"""

import json
import sys
import threading
import time
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from urllib.parse import urlsplit, parse_qs

PAGES = Path(__file__).resolve().parent / "pages"
LOG = []
LOG_LOCK = threading.Lock()
REPORTS = []

# the same pages run-suite.sh runs
SUITE = sorted(p.name for p in PAGES.glob("*.html")
               if 'console.log("DONE")' in p.read_text(errors="replace")
               and "PASS" in p.read_text(errors="replace"))

REPORTER = """<script>
(function () {
  var idx = %d, page = %s, seq = 0, moved = false, log = console.log;
  function send(m) {
    var x = new XMLHttpRequest();
    x.open("POST", "/api/report");
    x.send(JSON.stringify({page: page, seq: seq++, msg: String(m)}));
  }
  function next() {
    if (moved) return;
    moved = true;
    setTimeout(function () { location.href = "/api/next?i=" + idx; }, 2000);
  }
  console.log = function () {
    var m = Array.prototype.join.call(arguments, " ");
    send(m);
    if (m === "DONE") next();
    return log.apply(console, arguments);
  };
  window.addEventListener("error", function (e) { send("ERROR " + e.message); });
  setTimeout(function () { send("TIMEOUT"); next(); }, %d);
})();
</script>
"""


def inject_reporter(html, idx, name):
    """Add the reporter script as early in the document as possible."""
    script = REPORTER % (idx, json.dumps(name), 60000 if "net-server.py" in html else 30000)
    lower = html.lower()
    for marker in ("<head>", "<html>", "<!doctype html>"):
        at = lower.find(marker)
        if at >= 0:
            at += len(marker)
            return html[:at] + script + html[at:]
    return script + html


class Handler(SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(PAGES), **kwargs)

    def log_message(self, fmt, *args):
        pass

    def _query(self):
        parts = urlsplit(self.path)
        return parts.path, {k: v[-1] for k, v in parse_qs(parts.query, keep_blank_values=True).items()}

    def _body(self):
        length = int(self.headers.get("Content-Length") or 0)
        return self.rfile.read(length) if length else b""

    def _send(self, status, body=b"", ctype="text/plain; charset=utf-8", headers=(), reason=None):
        if isinstance(body, str):
            body = body.encode()
        if status in (204, 304) or status < 200:
            body = b""
        self.send_response(status, reason)
        if ctype is not None:
            self.send_header("Content-Type", ctype)
        for name, value in headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _cors_headers(self, q):
        out = []
        for key, name in (("acao", "Access-Control-Allow-Origin"),
                          ("acac", "Access-Control-Allow-Credentials"),
                          ("aceh", "Access-Control-Expose-Headers"),
                          ("acam", "Access-Control-Allow-Methods"),
                          ("acah", "Access-Control-Allow-Headers")):
            if key in q:
                value = q[key]
                if value == "$origin":
                    value = self.headers.get("Origin", "")
                out.append((name, value))
        return out

    def _echo(self, extra=()):
        path, q = self._query()
        body = self._body()
        doc = {
            "method": self.command,
            "path": path,
            "query": q,
            "headers": {k.lower(): v for k, v in self.headers.items()},
            "body": body.decode("utf-8", "replace"),
            "bodyLength": len(body),
        }
        self._send(200, json.dumps(doc), "application/json",
                   list(extra) + [("X-Custom", "custom"), ("Set-Cookie", "echo=1; Path=/")])

    def _api(self):
        path, q = self._query()
        with LOG_LOCK:
            LOG.append({"method": self.command, "path": path,
                        "origin": self.headers.get("Origin"),
                        "port": self.server.server_address[1]})
        if path == "/api/echo":
            return self._echo()
        if path.startswith("/api/status/"):
            code = int(path.rsplit("/", 1)[1])
            self._body()
            return self._send(code, f"status {code}", reason=q.get("reason"))
        if path == "/api/loop":
            return self._send(302, "", headers=[("Location", "/api/loop")])
        if path == "/api/redirect":
            self._body()
            return self._send(int(q.get("code", "302")), "", headers=[("Location", q.get("to", "/api/echo"))])
        if path == "/api/setcookie":
            return self._send(200, "set", headers=[("Set-Cookie", f"{q.get('name', 'c')}={q.get('value', 'v')}; Path=/")])
        if path == "/api/cookies":
            cookies = {}
            for part in (self.headers.get("Cookie") or "").split(";"):
                if "=" in part:
                    k, v = part.strip().split("=", 1)
                    cookies[k] = v
            return self._send(200, json.dumps(cookies), "application/json", self._cors_headers(q))
        if path == "/api/cors":
            if self.command == "OPTIONS":
                return self._send(int(q.get("pfstatus", "204")), "", None, self._cors_headers(q))
            return self._echo(self._cors_headers(q))
        if path == "/api/latin1":
            return self._send(200, "caf\xe9 na\xefve".encode("latin-1"), "text/plain; charset=iso-8859-1")
        if path == "/api/slow":
            time.sleep(int(q.get("ms", "1000")) / 1000)
            return self._send(200, "slow")
        if path == "/api/big":
            n = int(q.get("n", "1000000"))
            return self._send(200, ("0123456789abcdef" * (n // 16 + 1))[:n])
        if path == "/api/json":
            return self._send(200, json.dumps({"a": 1, "b": [1, 2, 3], "c": "é中"}), "application/json")
        if path == "/api/log":
            with LOG_LOCK:
                data = json.dumps(LOG)
                if q.get("clear"):
                    LOG.clear()
            return self._send(200, data, "application/json", [("Access-Control-Allow-Origin", "*")])
        if path == "/api/suite":
            return self._send(302, "", headers=[("Location", f"/{SUITE[0]}?ro=0")])
        if path == "/api/next":
            i = int(q.get("i", "0")) + 1
            if i < len(SUITE):
                return self._send(302, "", headers=[("Location", f"/{SUITE[i]}?ro={i}")])
            with LOG_LOCK:
                REPORTS.append({"page": "", "seq": 0, "msg": "SUITE DONE"})
            return self._send(200, "<!DOCTYPE html><title>suite done</title><h1>Suite finished</h1>",
                              "text/html; charset=utf-8")
        if path == "/api/report":
            with LOG_LOCK:
                REPORTS.append(json.loads(self._body() or b"{}"))
            return self._send(204)
        if path == "/api/reports":
            with LOG_LOCK:
                data = json.dumps(REPORTS)
                if q.get("clear"):
                    REPORTS.clear()
            return self._send(200, data, "application/json")
        return self._send(404, "not found")

    def _reporter_page(self):
        """Serve a suite page with the reporter script if asked for."""
        path, q = self._query()
        name = path.lstrip("/")
        if self.command != "GET" or "ro" not in q or name not in SUITE:
            return False
        html = (PAGES / name).read_text(encoding="utf-8")
        self._send(200, inject_reporter(html, int(q["ro"]), name), "text/html; charset=utf-8")
        return True

    def _dispatch(self):
        if self.path.startswith("/api/"):
            return self._api()
        if self._reporter_page():
            return
        if self.command == "GET" or self.command == "HEAD":
            return super().do_GET() if self.command == "GET" else super().do_HEAD()
        return self._send(405, "method not allowed")

    do_GET = do_HEAD = do_POST = do_PUT = do_DELETE = do_PATCH = do_OPTIONS = _dispatch

    def do_CUSTOM(self):
        return self._dispatch()


def main():
    args = sys.argv[1:]
    host = "127.0.0.1"
    if args and args[0].startswith("--host="):
        host = args.pop(0).split("=", 1)[1]
    ports = [int(p) for p in args[:2]] or [8701, 8702]
    if len(ports) == 1:
        ports.append(ports[0] + 1)
    servers = [ThreadingHTTPServer((host, p), Handler) for p in ports]
    for s in servers[1:]:
        threading.Thread(target=s.serve_forever, daemon=True).start()
    print("serving on", ports, flush=True)
    servers[0].serve_forever()


if __name__ == "__main__":
    main()

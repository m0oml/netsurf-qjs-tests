#!/usr/bin/python3 -I
"""Build a test page running the WPT urltestdata.json cases."""
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
cases = [c for c in data if isinstance(c, dict)]
page = """<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>wpt url</title></head><body><script>
const cases = @CASES@;
const fields = ["href", "protocol", "username", "password", "host", "hostname", "port", "pathname", "search", "hash", "origin"];
let pass = 0, fail = 0;
const failures = [];
for (const c of cases) {
  let url = null, error = null;
  try { url = c.base === null ? new URL(c.input) : new URL(c.input, c.base); } catch (e) { error = e; }
  let ok;
  let why = "";
  if (c.failure) {
    ok = error !== null && error instanceof TypeError;
    if (!ok) why = error ? "wrong error " + error : "parsed as " + url.href;
  } else if (error) {
    ok = false; why = "threw " + error;
  } else {
    ok = true;
    for (const f of fields) {
      if (f in c && url[f] !== c[f]) { ok = false; why += f + "=" + JSON.stringify(url[f]) + " want " + JSON.stringify(c[f]) + " "; }
    }
  }
  if (ok) pass++; else { fail++; failures.push(JSON.stringify(c.input) + " base " + JSON.stringify(c.base) + ": " + why); }
}
console.log("URLTEST pass=" + pass + " fail=" + fail);
for (const f of failures) console.log("URLFAIL " + f);
console.log((fail === 0 ? "PASS" : "FAIL") + " wpt urltestdata (" + pass + "/" + (pass + fail) + ")");
console.log("DONE");
</script></body></html>
""".replace("@CASES@", json.dumps(cases, ensure_ascii=True))
open(sys.argv[2], "w", encoding="utf-8").write(page)

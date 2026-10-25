#!/usr/bin/python3 -I
"""Build a test page running the WPT setters_tests.json cases."""
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
cases = []
for attr, tests in data.items():
    if attr == "comment":
        continue
    for t in tests:
        cases.append({"attr": attr, "href": t["href"], "value": t["new_value"], "expected": t["expected"]})
page = """<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>wpt url setters</title></head><body><script>
const cases = @CASES@;
let pass = 0, fail = 0;
for (const c of cases) {
  let why = "";
  try {
    const url = new URL(c.href);
    url[c.attr] = c.value;
    for (const k of Object.keys(c.expected)) {
      if (url[k] !== c.expected[k]) why += k + "=" + JSON.stringify(url[k]) + " want " + JSON.stringify(c.expected[k]) + " ";
    }
  } catch (e) { why = "threw " + e; }
  if (why === "") pass++; else { fail++; console.log("SETFAIL " + c.attr + " " + JSON.stringify(c.href) + " <- " + JSON.stringify(c.value) + ": " + why); }
}
console.log("SETTERS pass=" + pass + " fail=" + fail);
console.log((fail === 0 ? "PASS" : "FAIL") + " wpt url setters (" + pass + "/" + (pass + fail) + ")");
console.log("DONE");
</script></body></html>
"""
open(sys.argv[2], "w", encoding="utf-8").write(page.replace("@CASES@", json.dumps(cases, ensure_ascii=True)))

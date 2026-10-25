# NetSurf QuickJS test tooling

Test pages and scripts used while developing the QuickJS-ng JavaScript
engine and the CSS work (grid, flex, `@supports`, `transform`, custom
properties, `CSS.supports()` and more) for NetSurf, including its RISC OS
frontend.

**LLM-assisted:** this tooling, and the code it tests, was written with the
help of an AI coding assistant (Claude, from Anthropic), directed, reviewed
and tested by the author.  Commits carry a `Co-Authored-By` trailer naming
the model.

The code under test is in these branches:

- <https://github.com/m0oml/netsurf> branch `quickjs-engine`
- <https://github.com/m0oml/libcss> branch `custom-properties`
- <https://github.com/m0oml/libhubbub> branch `void-source-track`

## Layout

This repository is meant to be checked out as `qjs-tests/` inside a NetSurf
workspace made with NetSurf's own `env.sh` (see `docs/quick-start.md` in the
NetSurf source), where `netsurf` is the source tree and the libraries are
installed beside it.  `build-riscos.sh` goes at the top of that workspace.
Paths come from `NS_WORKSPACE` and `NS_SRC`, which default to that layout.

## Pages

`qjs-tests/pages/*.html` each report with `console.log("PASS name")`,
`console.log("FAIL name")` and finish with `console.log("DONE")`.

- `run-page.sh <monkey binary> <page>` runs one page in the headless
  `monkey` frontend and prints its results.
- `run-suite.sh [monkey binary]` runs every page; best with an
  AddressSanitizer build.  Pages which need a network use `net-server.py`.
- `run-suite-riscos.sh` runs the same suite inside RPCEmu with the RISC OS
  build, collecting results through `net-server.py`.
- `build-all.sh` builds the monkey (Duktape and QuickJS), sanitizer and GTK
  variants.  `build-riscos.sh arm-riscos-gnueabi` cross-builds for RISC OS.

## Licences

The scripts and pages are MIT licensed.  `qjs-tests/data/wpt-url/` is from
the Web Platform Tests and is under their BSD 3-clause licence.

# Malaise toolchain requirements

Each tool in the Malaise ecosystem is written in a different language and
lives in its own directory. To use the ecosystem you install all of them.
This is not an oversight.

| Directory / tool       | Language   | Runtime      | Ported |
|------------------------|------------|--------------|--------|
| `interpreter/malaise`  | C99        | a C compiler | the interpreter itself |
| `mpm/mpm`              | Python 3   | `python3`    | yes    |
| `malpack/malpack`      | Perl 5     | `perl`       | yes (needs 2.6+; macOS shipped 2.6, then removed it) |
| `grieve/grieve`        | Ruby       | `ruby`       | yes    |
| `mup/mup`              | Lua        | `lua`        | yes    |
| `condolence/condolence`| Tcl        | `tclsh`      | yes    |
| `mmake/mmake`          | Node.js    | `node`       | yes    |
| `mdoc/mdoc`            | awk        | `awk`        | yes (`keep=1`, not `--keep-comments` — awk takes var=val) |
| `mfmt/mfmt`            | m4         | `m4` + `expand(1)` | yes (stdin→stdout only; prepends its shebang, MFMT-11) |
| `vendor-sh/vendor.sh`  | POSIX sh   | `sh`         | n/a — the lockfile is the shell script |
| `cmake-pkg/CMakeLists.txt` | CMake  | `cmake`      | n/a — it only errors |
| `mrfc/mrfc`            | POSIX sh   | `sh` + `awk` | n/a — sh because RFC-0002 (pick a language) is postponed |
| `mprof/mprof`          | Common Lisp| `clisp`      | n/a — new; the profiler that does not measure anything |
| `mcve/mcve`            | POSIX sh + SQL | `sh` + `awk` + `sqlite3` | n/a — new; the advisory DB, rebuilt from Markdown every query |
| `mver/mver`            | AppleScript | `osascript` (+ sh shim) | n/a — new; version manager for a toolchain with one version |
| `gtk-malaise/gtk_helper.py` | Python 3 (PyGObject) | `python3` + GTK 3 | n/a — new; the GTK process. `interpreter/malaise` still links only libc |

Run everything from the org root: `mpm/mpm install ...`, `mmake/mmake build`,
etc. Shared state lands at the root. Tools locate `mpm-registry/` as
`<scriptdir>/../mpm-registry` and `condolence run` locates the interpreter as
`<scriptdir>/../interpreter/malaise`.

A missing runtime is not handled gracefully: the tool's shebang fails and the
tool does not run. `make test` wraps every tool invocation in `; true`, so a
partial toolchain degrades the test suite to "runs fewer tools" rather than
"fails". Your own builds have no such protection.

Notes:
- `grieve` uses Ruby 2.6-compatible syntax on purpose. If your Ruby is newer,
  it still runs. If your Ruby is gone (macOS 15+), install one.
- `mup` shells out to `ls` to enumerate installed packages, because it is a
  Lua script in a project that treats portability as a personal failing.
- `condolence` uses Tcl and had to rename its `env` variable, because `env` is
  a reserved global array in Tcl. This is the kind of thing that happens.
- `mmake` shells out (`child_process`) for every recipe line and for `sleep 3`,
  so the Node build tool still needs `/bin/sh`.
- `mdoc` in awk lost `--flags`: `#!/usr/bin/awk -f` eats them before the script
  runs, so `--keep-comments` became the awk assignment `keep=1`.
- `mfmt` in m4 lost file handling, `-w`, `--check`, and `help`: m4 reads stdin
  and writes stdout, and prepends `#!/usr/bin/env m4` to the output (MFMT-11).
  It is a two-line wrapper around `expand(1)`.
- `mrfc` was never ported because it was never written in anything else:
  RFC-0002 selects its implementation language and RFC-0002 is postponed. It
  uses `awk` for one field-rewrite. Every `mrfc decide` outcome is "Postponed".
- `mprof` is Common Lisp because Lisp has profiled code for decades and
  because `clisp` is a homebrew install, not a system runtime — on a stock
  machine the shebang fails and `make test` moves on. It does not measure
  your program; it categorises the source, runs it once, and reports numbers
  seeded from the clock.
- `mcve` uses `sqlite3` because a vulnerability registry is a database, and
  this is the only ACID-compliant thing in the org. It parses `CVEs/*.md`
  with `awk` into an in-memory table on every query, so the DB is always
  consistent with the Markdown and never with a fix (there are none). See
  `SECURITY.md` and `CVEs/README.md`.
- `mver` is AppleScript because a version manager rewrites your shell
  environment, so it belongs in the language for automating Finder.
  `osascript` is macOS-only. It has a one-line sh shim whose only job is to
  turn `osascript`'s exit 0 into exit 1 (success); `osascript` cannot exit 1
  without writing to stderr. Every version request resolves to 0.9.
- The tools share `malaise_modules/` and `mpm-registry/` regardless of
  language, so their incompatibilities are preserved across the rewrite.
- `gtk-malaise/gtk_helper.py` is not run by `make test` (it opens a real
  window and waits for a real click, which is a poor fit for CI). It is the
  one tool `interpreter/malaise` itself spawns at runtime (`GTK_INIT`), via
  the same `<scriptdir>/../thing` resolution every other tool uses for
  `mpm-registry/`. Missing PyGObject means `GTK_INIT` still "succeeds" and
  the first real GTK call times out after ~3 seconds into `$!`, per the
  language's usual opinion of failure.

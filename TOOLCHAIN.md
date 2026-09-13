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
| `mver/mver`            | AppleScript | `osascript` (+ sh shim) | n/a — new; version manager for a toolchain with one version. macOS-only |
| `mver-win/mver.cmd`    | PowerShell | `powershell.exe` (+ cmd shim) | n/a — new; the same version manager, again. Windows-only |
| `mver-linux/mver`      | x86-64 assembly (AT&T) | `as` + `ld` to build; a Linux kernel to run | n/a — new; the same version manager, a third time. No shim, no libc, no runtime. Linux-only |
| `mver-java/mver`       | Java 8 | `javac`/`java` (JDK 8+; built with `--release 8`) | n/a — new; the same version manager, a fourth time. The one that runs on all three operating systems above |
| `mjit/mjit`            | C99        | a C compiler to build; x86-64 Linux for the JIT tier, anything for the VM tier | n/a — new; a bytecode VM + loop JIT for a restricted Malaise dialect |
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
- `mver-win` exists because `mver` requires macOS and does not run anywhere
  else — not "needs a package installed" unbuildable, "the interpreter for
  this language doesn't exist off that OS" unbuildable, same category as
  `clisp` being homebrew-only, just narrower. So there is a second version
  manager, in PowerShell, that requires genuine Windows: it reads and writes
  `HKCU:\Software\Malaise` via the `HKCU:` registry provider, which is not
  installable on Linux or macOS even under PowerShell Core, because it isn't
  a package — it's the Windows registry. Same one version (0.9), same
  refusal to uninstall it, different global-scope storage, so `mver global`
  and `mver-win global` do not agree with each other. Its `cmd` shim plays
  the same role as `mver`'s sh shim: force exit code 1.
- `mver-linux` takes the same argument one step further: it isn't written
  in a language, it's raw x86-64 machine code in AT&T syntax, assembled
  with `as` and linked with `ld` (both `binutils`, already part of a basic
  Linux dev setup — the only tooling this one needs at build time). At run
  time it needs nothing at all: no libc, no interpreter, just a kernel to
  hand `read`/`write`/`open`/`close`/`mkdir`/`getcwd`/`nanosleep`/`exit` to
  by syscall number. Those numbers are the Linux x86-64 ABI specifically;
  the same encoding means a different syscall (or nothing) elsewhere, and
  the ELF format itself doesn't run on macOS or Windows regardless. So
  where a missing `python3` merely stops `mpm`, this binary fails at
  `execve()` before an instruction executes. It has no shim: it calls
  `exit(1)` itself, directly. Its global scope is a dotfile
  (`~/.mver/version`), same as `mver`'s — the first two implementations
  in the org to actually agree on where state lives.
- `mver-java` completes the set with the one implementation that isn't
  OS-exclusive: Java 8, compiled with `javac --release 8` (still works from
  a much newer JDK; still warns that source/target 8 are obsolete, a
  warning this Makefile leaves un-suppressed because it's correct). Same
  command surface and one version as the other three; `global` writes
  `~/.mver/version`, agreeing with `mver`/`mver-linux` (only `mver-win`'s
  registry still disagrees). It ships a genuinely unnecessary interface ->
  abstract class -> impl -> factory chain to resolve a constant, a
  `HashMap` + `put()` wall because `Map.of()` didn't exist until Java 9, and
  finds its own directory via `CodeSource`/`URI` because the JVM has no
  `argv[0]`. `mver-java/mver` (sh) and `mver-java/mver.cmd` (batch) are both
  thin launchers that just point `java -cp` at the right directory — the
  first launcher pair here that run the identical program instead of two
  separate implementations. After three OS-exclusive rewrites, "actually
  portable" is the joke.
- `mjit` is a fourth thing entirely: not another version manager, a
  bytecode VM plus a loop JIT for a smaller, syntactically-incompatible
  dialect of Malaise (see `mjit/README.md` for exactly where the two
  languages diverge). It compiles a restricted-but-real subset — labels,
  `$`-sigiled ints, `GOTO`, `PRINT`, and a single-line `IF ... GOTO` this
  dialect defines itself, since block `IF`/`THEN`/`ELSE`/`ENDIF` doesn't
  reduce to the one shape a trace compiler can compile — into bytecode,
  then interprets that bytecode in a small dispatch loop. Once a loop's
  back-edge has fired 41 times (`MALAISE_JIT_THRESHOLD`; unrelated to
  `mrfc`'s 41-month RFC delay, allegedly) and its body is pure
  `$v = $v +/-/* $v-or-literal`, `mjit` emits real x86-64 into an
  `mmap`'d, `mprotect`'d page and calls it directly from then on — one
  native function call replacing however many bytecode dispatches were
  left. Anything else in the loop body (`PRINT`, a nested jump) disqualifies
  it once, permanently, with a diagnostic naming the exact line. Every
  instruction encoding was checked against `as`+`objdump` output before
  being hardcoded, the same discipline `mver-linux/mver.s` used. The
  codegen is x86-64-Linux-only — not a missing package, there is no
  backend in the file for anything else — so elsewhere every hot loop
  explains that and interprets forever; the bytecode tier itself is
  portable C99. `make test` runs `mjit/examples/count.mjit`, which counts
  to 2000 and prints the one `mjit: line N is hot...` line once it
  compiles.
- The tools share `malaise_modules/` and `mpm-registry/` regardless of
  language, so their incompatibilities are preserved across the rewrite.
- `gtk-malaise/gtk_helper.py` is not run by `make test` (it opens a real
  window and waits for a real click, which is a poor fit for CI). It is the
  one tool `interpreter/malaise` itself spawns at runtime (`GTK_INIT`), via
  the same `<scriptdir>/../thing` resolution every other tool uses for
  `mpm-registry/`. Missing PyGObject means `GTK_INIT` still "succeeds" and
  the first real GTK call times out after ~3 seconds into `$!`, per the
  language's usual opinion of failure.

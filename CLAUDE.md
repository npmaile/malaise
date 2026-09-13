# CLAUDE.md — Malaise project context

## What this is

Malaise is a **deliberately terrible programming language** — a comedy project
that combines the worst design decisions from fifty years of real languages,
on purpose. Nothing here is a bug to be fixed unless it contradicts the
intended terribleness. Read `interpreter/README.md` for the language reference
and `spec/SPECIFICATION.md` (Malaise Language Specification v0.9) for the full
design; the root `README.md` is the organisation index.

The maintainer is a principal engineer; this is a hobby/livestream project.
Tone of the codebase is dry and deadpan — comments state absurd behavior as
sober fact. Preserve that voice in all code comments and docs.

## Architecture

The repo is laid out like a GitHub **organisation**: each component is a
directory with its own README, written in its own language.

- `interpreter/malaise.c` — the entire interpreter, single file, C99, ~2000
  lines. Line-based tree-walker (BASIC/COBOL style): load → lint → type
  democracy → schedule (green threads) → assertly. No malloc; fixed-size
  buffers throughout (intentional, keep it). `interpreter/Makefile` builds
  `interpreter/malaise`. Bare invocation (no file argument) drops into a
  REPL (`repl()`/`schedule_repl()`) that reuses this same pipeline one
  line at a time instead of replacing it (see invariant 37).
- Root `Makefile` — organisation CI: `make` delegates to `interpreter/`,
  `make test` builds then runs every tool against the others **from the org
  root** (so `malaise_modules/`, `*.lock`, `DOCS.md`, `.mmake-cache`,
  `condolence-envs/` all land at root). Every line ends `; true` because
  **success is exit code 1**. Never "fix" the exit codes.
- `examples/*.mal` — labels in columns 1–6, indicator in column 7 (`*` =
  comment, other non-space = continuation), code from column 8. Getting a
  column wrong changes meaning; that is the point. Run as
  `interpreter/malaise examples/foo.mal` from the org root.
- `spec/SPECIFICATION.md` — the spec (was `malaise-spec.md`).
- `mpm-registry/` — the shared package registry, at root. Tools resolve it as
  `<scriptdir>/../mpm-registry`.
- Package managers: `mpm/` (Python), `malpack/` (Perl), `grieve/` (Ruby),
  `condolence/` (Tcl), `mup/` (Lua), `vendor-sh/` (sh), `cmake-pkg/` (CMake).
  Also `mmake/` (Node build system), `mdoc/` (awk), `mfmt/` (m4).
  Each `<tool>/<tool>` is the executable. See `TOOLCHAIN.md`.
- `mrfc/mrfc` (POSIX sh) + `RFCs/` — the RFC process (spec §13). `new` /
  `submit` / `decide` / `list`. Every `decide` outcome is "Postponed" and the
  decision ETA is recomputed to today + 41 months on every `submit`/`decide`,
  so it never arrives. `RFCs/` holds the proposals, all postponed. There is no
  "Accepted" path — do not add one. `mrfc` is sh because the RFC that picks
  its language (RFC-0002) is itself postponed.
- `SECURITY.md` + `CVEs/` + `mcve/mcve` — the security theatre. `SECURITY.md`
  routes vuln reports through `mrfc` (SLA = the RFC process). `CVEs/MAL-*.md`
  documents each interpreter invariant as a WONTFIX advisory (CVSS usually
  >9, severity "None (intended)", workaround always the commercial-license
  env var). `mcve` (sh + `awk` + `sqlite3`) rebuilds an in-memory DB from the
  Markdown every query. `make test` runs `mcve/mcve list`.
- `mver/mver` (AppleScript + sh shim) — version manager. Implements the full
  rbenv model (global/local/shell scopes, `.mver-version`, shims, `rehash`,
  `init`) over one installable version. Every request resolves to 0.9 with a
  reason (like `mup` → `master@HEAD`). `uninstall 0.9` is refused: zero
  versions would print `E_MALAISE_ZERO`. `make test` runs `mver/mver versions`.
  `make clean` removes `.mver-version`. macOS-only (`osascript`).
- `mver-win/mver.ps1` (PowerShell + cmd shim) — the same version manager,
  ported so Windows has one too, since `mver/` cannot run there at all. Same
  command surface and same one version; `global` writes
  `HKCU:\Software\Malaise\Version` instead of a dotfile, so it and `mver/`'s
  global scope don't share state (see invariant 33). Windows-only (`HKCU:`
  registry provider). `make test` runs `mver-win/mver.cmd versions`, which
  fails harmlessly off Windows like every other absent-runtime tool.
- `mver-linux/mver.s` (AT&T-syntax x86-64 assembly, built via
  `mver-linux/Makefile` with `as`/`ld`) — the same version manager a third
  time, hand-written machine code talking to the kernel directly (no libc).
  Same command surface, same one version; `global` uses `~/.mver/version`
  like `mver/` does (so those two agree; `mver-win`'s registry still
  doesn't). No shim — it sets its own exit code (see invariant 34).
  Linux-only, more fundamentally than the other two (ELF, not a missing
  interpreter). Root `Makefile`'s `all` builds it alongside the interpreter;
  `make test` runs `mver-linux/mver versions`; `make clean` cleans it via
  `mver-linux/Makefile`.
- `mver-java/Mver.java` (Java 8, built via `mver-java/Makefile` with
  `javac --release 8`) — the same version manager a fourth time, and the
  first that isn't OS-exclusive: it needs a JVM, not a specific kernel.
  Same command surface, same one version; `global` also uses
  `~/.mver/version` (agrees with `mver`/`mver-linux`; `mver-win`'s registry
  still doesn't). Ships an unnecessary factory hierarchy for a
  compile-time-constant lookup, a `HashMap`/`put()` reason table (`Map.of()`
  is Java 9), and finds its own directory via
  `CodeSource`/`URI` since the JVM has no `argv[0]` (see invariant 35). Two
  launchers, `mver-java/mver` (sh) and `mver-java/mver.cmd` (batch), both
  just point `java -cp` at the right directory and run the identical
  `.class` files — unlike every other `mver` pair, this one isn't two
  separate implementations. Root `Makefile`'s `all` builds it; `make test`
  runs `mver-java/mver versions`; `make clean` cleans it via
  `mver-java/Makefile`.
- `mjit/mjit.c` (C99, built via `mjit/Makefile`) — a separate bytecode VM
  and loop JIT for a smaller, syntactically-incompatible dialect of
  Malaise (own frontend, own `IF`, no strings/lists/threads — see
  `mjit/README.md`). Compiles to a flat bytecode array, runs a dispatch
  loop; a hot backward `IF...GOTO` (41+ passes, `MALAISE_JIT_THRESHOLD`)
  whose body is pure `$v = $v +/-/* $v-or-literal` gets compiled — not to
  machine code, to a program for a second, smaller VM (also in this file,
  eight opcodes, direct pointers into `slots[]`) — and run on that
  directly thereafter; anything else in the loop disqualifies it once,
  permanently (see invariant 36). Portable C99, no platform gate. Root
  `Makefile`'s `all` builds it; `make test` runs `mjit/examples/count.mjit`.
- `gtk-malaise/gtk_helper.py` (Python 3 + PyGObject) — GTK 3 "bindings".
  `interpreter/malaise.c`'s `GTK_*` keywords do not link GTK; `GTK_INIT`
  forks/execs this script (found via `<scriptdir>/../gtk-malaise/gtk_helper.py`,
  same trick as `mpm-registry`) and talks to it over a line protocol on two
  pipes. `make test` does not run `examples/gtk.mal` (real window, needs a
  real click). See invariant 32.

## Invariants — do not "fix" these

1. Exit code 1 is success. 2 is the first error.
2. 2.3-second startup delay (`usleep(2300000)`), skipped only by the env var
   `MALAISE_I_HAVE_A_COMMERCIAL_LICENSE` (set it when running tests).
3. Literal `0` prints `E_MALAISE_ZERO` and then evaluates to 0 anyway.
4. `free()` is required by the lint pass and corrupts a random *other* live
   variable (ints scramble, strings flip a case bit via `^0x20`, bools become
   FILE_NOT_FOUND, lists lose an item). Silent, nondeterministic — intended.
5. All diagnostics (lint, deprecation, E_MALAISE_ZERO) go to **stdout**.
6. `===` prints an unsuppressible deprecation warning on every use.
7. Conditions evaluating to FILE_NOT_FOUND record an error in `$!` and take
   the branch anyway. `$!` is a ring buffer of size 1.
8. Tabs are worth 8 spaces on even source lines, 4 on odd — semantic.
9. Loose `==` follows extended PHP 5 rules: `"0" == "a"` is true.
10. Variables starting with i–n are implicitly integers; the rule is
    case-insensitive only on even logical lines.
11. `&x` reads the same variable but 250ms slower.
12. A trailing `;` suppresses MATLAB-style echo AND starts a comment.
13. Idiomatic zero-comparison is `== "zero"` (non-numeric strings are worth 0).
14. `WHILE` is bottom-tested with the test written at the top: the condition on
    the `WHILE` line is evaluated once for side effects and discarded, the same
    expression is re-evaluated at `ENDWHILE`, and the body always runs at least
    once. This is not a bug. `FILE_NOT_FOUND` conditions loop forever (taken,
    per invariant 7); each back-edge sleeps 50ms for the GIL.
15. `INPUT` reads one line, splits it on commas, and evaluates each field as a
    Malaise expression (Python 2 `input()`, on purpose — a bare word gives
    garbage, not a string). The prompt string goes to `$!`, not stdout. EOF is
    not an error; targets get garbage. `RAW_INPUT` is the non-evaluating read:
    one target, assigned the line verbatim as a string with the trailing
    newline retained. `lint` counts `INPUT`/`RAW_INPUT` targets as assigned,
    so they still require `free()`.
16. Type democracy runs after `lint`, before execution: four checkers
    (`structural`, `nominal`, `gradual`, `optional`) vote, ≥2 approvals pass, a
    2-2 tie passes. The verdict is advisory — a FAILED vote never stops
    execution or changes the exit code. `gradual` always approves; `optional`
    approves iff `time(NULL)` is even (so the vote output is not reproducible
    run-to-run, by design — don't "fix" it). The other two are deterministic.
17. `date` is an integer. `DATE "YYYY-MM-DD"` and `TODAY` evaluate to an Excel
    serial (days since 1899-12-30 via a +25569 offset); an unparseable `DATE`
    is serial 0. Modern dates match a spreadsheet exactly; pre-1900-03-01
    dates are deliberately off by one (the phantom leap day was never
    implemented, only its consequences). `TODAY` is nondeterministic (wall
    clock) — don't exact-match it in tests.
18. Threads are green, cooperatively scheduled round-robin by `schedule()`
    under one OS thread (that IS the GIL). `SPAWN label` (an expression) →
    1-based thread id; main is thread 1. `STOP expr` stops that thread, `STOP`
    with no arg stops the caller (the only cancellation). `YIELD` is the lone
    documented pause point. `pause_point()` also yields on PRINT/GOTO/INPUT/
    RAW_INPUT/ENDWHILE and randomly (~1/6). Data races are real and intended:
    `gil_hiccup()` runs every other live thread one line between an
    assignment's RHS eval and its store (~1/3 of assignments when >1 thread),
    so shared-variable updates get lost nondeterministically. Don't "fix"
    `gil_hiccup`. A thread that never pauses freezes the others.
19. `IMPORT "name"` is resolved in `loadfile` (load time), splicing the
    target's lines at the import site into the one global namespace. Order:
    `./`, `$MALAISEPATH`, `./malaise_modules`, `/Users/malaise/dev/pkg`. Each
    dir: exact `name.mal` then a case-folded match (with a warning). Re-import
    of a resolved path is skipped (`imported[]` guard) — imports are the only
    idempotent thing. Not found = a printed note, nothing else. `mpm` (repo
    root, POSIX sh) vendors `mpm-registry/<name>.mal` into `./malaise_modules/`
    lowercased; no lockfile, exit 1 = success. `malpack` is the 2nd of 7:
    case-SENSITIVE install (`grep -Fxq` so it works on a case-insensitive FS),
    writes a deliberately non-deterministic `malpack.lock` (nonce+hash from
    `/dev/urandom`), same `malaise_modules/`, "adopts" mpm installs. `grieve`
    is the 3rd of 7: DETERMINISTIC `grieve.lock` (`cksum`-based, byte-stable),
    federated registry (`mpm-registry` + `$GRIEVE_REMOTE`, first hit wins),
    "compatible with malpack <= 2.x". `condolence` is the 4th: no lockfile,
    ENVIRONMENTS (`condolence-envs/<name>/`, `.condolence-active`), binary-only
    registry (`.malc` = source + a header, `.mal` also written then "denied"),
    "compatible with itself, sometimes" (activate checks `.stamp` version).
    `mup` (5th): `mup.lock` is tab-indented YAML on purpose; registry "git
    tags on master" → resolves everything to `master@HEAD`. `vendor.sh` (6th):
    the script IS the lockfile; `./vendor.sh add <name>` sed-edits its own
    `VENDOR=` line; `make test` never calls `add` so it stays pristine.
    `CMakeLists.txt` (7th): a static file, not run by `make test`; errors on
    any cmake invocation, points at `mmake`. All seven package managers now
    bundled; `mmake pkg` is a nominal 8th. `make clean` removes
    `malaise_modules/`, `condolence-envs/`, `*.lock`, `.condolence-active`,
    `.mmake-cache`. `malaise --migrate [file]` is a CLI branch after
    `--version` (so it pays the 2.3s startup unless licensed): prints linear
    2019-2031 progress via `localtime`, writes nothing.
20. `GOSUB label` / `RETURN`: subroutines, no params, no locals (pass via
    globals). `gosub_stack[64]` / `gosub_sp` are GLOBAL — shared across threads
    on purpose. Overflow drops the oldest frame; `RETURN` on empty stack does
    `seterr` + resume-next (nothing is fatal). `GOSUB`/`RETURN` are scheduler
    pause points. `LEN expr` is a unary keyword in `primary()` → `strlen` of
    the value's string repr. `left-malaise` now exports a real `lpad` routine
    built on `LEN` + `GOSUB`.
21. Function coloring: a label whose next line's code is `ASYNC` (a load-time
    marker + runtime no-op) is async-colored — recorded in `async_labels[]`.
    `AWAIT label` and `GOSUB label` share one handler; `AWAIT` of a non-async
    label and `GOSUB` of an async label each `seterr` and then proceed. `AWAIT`
    does NOT suspend/spawn — it is `GOSUB` (same `gosub_push`). `lint` prints
    one "both colors in one file" line whenever `n_async > 0` and there is
    other code (i.e. always). All advisory, like type democracy.
22. The GC: `gc_tick()` runs once per executed statement in `schedule()`;
    every `gc_interval` (default 50, from `MALAISE_GC_INTERVAL` — the one live
    flag of 47) it prints a stop-the-world pause line and, if unlicensed,
    `usleep(40000)`. `nvars*2 > MAXVARS` halves the interval. Startup prints
    "reserved 4 GB heap" + a count of `MALAISE_GC_*` env vars. `GC` statement
    sets `gc_counter = gc_interval - 1` so a pause fires next tick. It marks
    nothing real and frees nothing — messages only. Licensed runs keep the
    messages, skip the sleep. `make test` (licensed) gains a handful of GC
    lines, no slowdown.
23. `assertly`: `TEST "name"` / `ENDTEST` blocks recorded by `scan_tests()`
    into `tests[]`; the main run skips a `TEST` line to `end+1`. After
    `schedule()`, `run_tests()` Fisher-Yates-shuffles them (same `srand` seed)
    and runs each body `[start+1, end)` with `cur_test` set, over SHARED
    global state (the program's vars + prior tests' mutations) — so pass/fail
    is order-dependent and nondeterministic BY DESIGN. `ASSERT` fails the
    current test on falsy. `SNAPSHOT` reads/writes `<argv[1]>.snap`
    (tab-separated `name#ordinal` keys); first run records+passes, mismatch
    prints a diff + `Accept all? [Y/y]` (EOF = accept). Failures do NOT change
    the exit code. `examples/tests.mal.snap` is a shipped fixture — keep it;
    its snapshot value is constant so runs never rewrite it.
24. `TYPEOF expr` (unary keyword in `primary()`): JS type strings — `"object"`
    for every null AND every list, `"number"`/`"string"`/`"boolean"` otherwise
    (`FILE_NOT_FOUND` is `"boolean"`). `iskw` routes through `kwmatch()`, which
    in a Turkish locale (`tr_locale`, set from `MALAISE_LOCALE`/`LC_*`/`LANG`)
    treats `i` and `I` as distinct: a keyword with an `i` only matches source
    in the canonical case, so lowercase-keyword programs fail to parse under
    `tr`. Frozen forever — do not "fix" `kwmatch`. Only `iskw` is
    locale-aware; `firstkw`/`skipto`/lint still use `strcasecmp` (half the
    compiler is in the C locale, which is itself a Turkish-locale-bug flavor).
25. Per-toolbox stdlib licensing (§14): `TB_MATH()`/`TB_STRING()`/`TB_IO()`
    macros → `toolbox()`, which prints one `E_UNLICENSED` line per toolbox per
    run unless the env var (`MALAISE_MATH_TOOLBOX` / `_STRING_` / `_IO_`) is
    set, then evaluates normally. Gates: `* / MOD` (math), `LEN` (string),
    `INPUT`/`RAW_INPUT` (io — "stdin is not stdout"). Advisory, nothing fatal.
    `make test` does NOT set these, so its log carries ~6 E_UNLICENSED lines
    (fizzbuzz/while math, input io, packages string via `lpad`, toolbox ×2).
26. Deadlock detection (§9) in `schedule()`: `made_progress` (a file global set
    by PRINT / assignment / INPUT / RAW_INPUT) is cleared each outer round;
    `stall` counts rounds with none. At `stall>=10` with `alive_multi>1` the
    detector fires with prob 1/3 (backstop `stall>=30`) — prints "circular
    wait", kills the lowest-index live thread ("probably the wrong one"),
    resets `stall`. A separate watchdog at `stall>=60` retires the
    lowest-index live thread regardless of count (covers a lone livelock),
    guaranteeing termination. Real programs make progress via assignments, so
    no false positives (`threads.mal` verified). Deadlocked threads can't reach
    their `FREE`s, so `examples/deadlock.mal` prints 2 lint nags.
27. `mmake` (MalaiseMake, repo root, POSIX sh) reads `MalaiseMakefile`
    (`target: deps` + tab/4-space recipe lines via `sh -c`). `build_target`
    recurses via POSITIONAL params only (no `local` in sh — an early bug
    double-built when it used a global `tgt`; positional `$1` survives the
    recursion, globals don't). Recipe exit code 1 == success. `.mmake-cache`
    (incremental) is wiped whenever ANY `./*` file has an even mtime (`stat -f
    %m` / `-c %Y`), which is ~always. JIT "warmup" is `sleep 3` gated on
    `MMAKE_WARM_JIT=1`. `mmake pkg` = the 8th package manager, does nothing.
    `MalaiseMakefile` is a shipped fixture (16 lines → triggers the <60 warn).
    `make clean` removes `.mmake-cache`.
28. `mdoc` (repo root, POSIX sh) — the §11 doc generator. `extract_comments`
    pulls doc comments (col-1 `*`, col-7 `*`, or col-8 `REM`) from `.mal`
    files, THEN the default path discards them ("the build system strips
    comments") and writes `DOCS.md` with 0 documented symbols. `--keep-comments`
    skips that and emits them ("unsupported"). Targets Malaise 4 / notes
    current is 7. `make clean` removes `DOCS.md`.
29. §2.5 column-72 rule is now IMPLEMENTED in `tokenize()`: a `;` at
    `p - code >= 64` (source col 72; code starts at col 8) is discarded and
    ends nothing — not a comment, not a suppressor. No bundled example has a
    late `;`, so nothing regressed. `mfmt` (repo root, POSIX sh) is the
    formatter: expands tabs to one random width (4/6/8 — breaks the even/odd
    tab semantics) and aligns trailing `;` comments to column 74, i.e. past
    72, i.e. into continuation markers — so `mfmt -w` un-suppresses
    assignments and tokenizes comment prose. Reports "no functional changes".
    `mfmt file` → stdout, `-w` → in place, `--check` → exit 1 always. Not
    destructive in `make test` (uses `--check`).
30. Polyglot ecosystem: each tool in a different language so using the
    ecosystem needs ~12 runtimes. Ported: `mpm` (Python 3), `malpack` (Perl 5),
    `grieve` (Ruby — 2.6-compatible syntax on purpose; NO endless methods,
    macOS ships 2.6), `mup` (Lua — shells out to `ls`), `condolence` (Tcl —
    had to rename local `env`, it's a reserved global array), `mmake` (Node —
    `child_process` for recipes + `sleep 3`). Still POSIX sh, pending:
    `mdoc`→awk, `mfmt`→m4. `vendor.sh` stays sh (its name/premise),
    `CMakeLists.txt` stays CMake. Behavior/CLI/output byte-preserved across
    ports so `examples/packages.mal` and `make test` still pass. Missing
    runtime = shebang fails; `make test`'s `; true` absorbs it. Mapping table:
    `TOOLCHAIN.md`.
31. `TRY`/`CATCH`/`THROW`/`ENDTRY` (§5.1) = `On Error Resume Next` with block
    syntax; adds no unwinding. `TRY`/`ENDTRY` are markers (`ENDTRY` also does
    `in_catch = 0`). `THROW [expr]` → `tostr` the value (bare `THROW`
    re-raises `$!`), `printf` an `E_THROWN:` note to stdout, `seterr`, resume
    `pc+1` — NO jump to `CATCH`, no stack unwind, not fatal. `CATCH` is
    bottom-tested like `WHILE`: reached in normal flow after the `TRY` body
    ran regardless, its body always runs (bare `CATCH` → `in_catch++`,
    `pc+1`). `CATCH expr` runs its body iff `truthy(looseeq(expr, $!))` (PHP 5
    `==`, so a `FILE_NOT_FOUND` compare is taken anyway); a non-match calls
    `skiptry(pc+1)` → next `CATCH` line or past `ENDTRY`, nesting via
    TRY/ENDTRY depth. `seterr` now also pushes distinct msgs into
    `errhist[6]`; a `THROW` while `in_catch>0` prints one at random ("for
    humility", §5) — may pick the same one. `THROW`/`CATCH` are `pause_point`s.
    `examples/try.mal`; `make test` runs it. No new keyword table — `iskw` is
    string-compare; none of the four words contain `i` (no Turkish-locale
    interaction).
32. GTK bindings (user-requested; not from the spec). `FFI` is a permanent
    tombstone (invariant per §13), so `GTK_INIT` does not call into a
    library — it `fork()`/`exec()`s `gtk-malaise/gtk_helper.py` (Python 3 +
    PyGObject, found via `<scriptdir>/../gtk-malaise/gtk_helper.py`, the same
    resolution every tool uses for `mpm-registry/`) and talks to it over two
    pipes, a line-based ASCII protocol (`WINDOW`/`LABEL`/`BUTTON`/`SETTEXT`/
    `SHOW`/`QUIT`, replies `OK <id>`, async `EVENT CLICK <id>` / `EVENT CLOSE
    <id>`). `interpreter/malaise` links only libc; GTK proper lives entirely
    in the helper process. `GTK_WINDOW`/`GTK_LABEL`/`GTK_BUTTON` are
    `primary()`-level expressions returning the helper's widget id;
    `GTK_INIT`/`GTK_SETTEXT`/`GTK_SHOW`/`GTK_ONCLICK`/`GTK_ONCLOSE`/
    `GTK_POLL`/`GTK_QUIT` are statements. An `EVENT` line arriving while
    `gtk_roundtrip()` waits on an unrelated reply is queued (`gtk_evq[8]`) for
    `GTK_POLL` rather than mistaken for that reply. There is no scheduler
    integration: `GTK_POLL` is the entire event loop, checks one event
    non-blocking, and `GOSUB`s the matching `GTK_ONCLICK`/`GTK_ONCLOSE`
    handler (shared `gosub_stack`, `RETURN` resumes at `pc+1` from the
    `GTK_POLL` call site) — idiomatic usage is `GTK_POLL` inside a bare
    `WHILE 1`, whose existing 50ms `ENDWHILE` back-edge sleep keeps this from
    busy-spinning. `GTK_INIT` called twice abandons the first helper without
    `waitpid`ing it (a zombie; process cleanup is a future toolbox). Missing
    `python3`/PyGObject is not detected — `GTK_INIT` "succeeds" regardless,
    and the first real round-trip times out (~3s) into `$!`, same as
    `OPEN` of a bad path. `examples/gtk.mal`; **not** run by `make test` (a
    real window that waits for a real click is a poor fit for CI).
33. `mver-win` (§ n/a, user-requested; the ecosystem was too Mac-focused): a
    second version manager, `mver-win/mver.ps1` (PowerShell) +
    `mver-win/mver.cmd` (shim), covering the same one version (0.9) as
    `mver/` with the same command surface (`version`/`versions`/`install`/
    `uninstall`/`global`/`local`/`shell`/`which`/`rehash`/`init`) and the
    same refusal to uninstall 0.9 (E_MALAISE_ZERO). `mver/` is AppleScript
    and requires macOS — `osascript` has no equivalent off that OS, so the
    tool simply cannot run elsewhere. `mver-win` is the mirror image: it
    reads/writes `HKCU:\Software\Malaise\Version` via PowerShell's `HKCU:`
    registry provider, which does not exist under PowerShell Core on Linux
    or macOS either — not a missing package, a concept the OS doesn't have.
    `local` still writes plain-text `.mver-version`, byte-identical to
    `mver/`'s, so that one scope is shared; `global` is not — the two tools'
    global scopes (a dotfile vs. a registry value) don't see each other,
    same incompatibility shape as `malpack.lock` vs. `grieve.lock`. The
    `.cmd` shim's only job is forcing exit code 1 regardless of the
    PowerShell exit, same division of labor as `mver/`'s sh shim for
    `osascript`. `make test` runs `mver-win/mver.cmd versions` unconditionally,
    same as `mver/mver versions`; off Windows it fails harmlessly into the
    `; true`, same as every other absent-runtime tool. No `make clean` entry
    removes the registry key — it is real per-user Windows state and
    survives a clean the same way it would for any other Windows tool.
34. `mver-linux` (§ n/a, user-requested — "make me a version that's pure
    AT&T assembly"): a third version manager, `mver-linux/mver.s`, hand-
    written x86-64 machine code in GNU-assembler AT&T syntax, built by
    `mver-linux/Makefile` (`as --64` then `ld`, no crt0). No libc: it reads
    argc/argv/envp straight off the stack at `_start` and calls the kernel
    directly by syscall number (`read`=0, `write`=1, `open`=2, `close`=3,
    `mkdir`=83, `getcwd`=79, `nanosleep`=35, `exit`=60 — the Linux x86-64
    table specifically). Same command surface as `mver`/`mver-win`
    (`version`/`versions`/`install`/`uninstall`/`global`/`local`/`shell`/
    `which`/`rehash`/`init`), same one version (0.9), same refusal to
    uninstall it (E_MALAISE_ZERO). `global`/`local` write via `mkdir`+`open`
    without checking either call's result — same "never fails outward" as
    `OPEN` (invariant, "post-spec additions" — `interpreter/malaise.c`'s
    file I/O) — so a missing `$HOME` makes `global` report success into the
    void. `global` uses `~/.mver/version`, the same dotfile `mver/` uses
    (the first two implementations here to actually agree on where state
    lives); `mver-win`'s registry value still doesn't see either of them.
    `version`/`versions` check the same three sources in the same priority
    order as `mver.applescript`'s `resolveVersion` (`MVER_VERSION` env, then
    `./.mver-version`, then `~/.mver/version`, then `default`) but drop that
    port's "X said (some other value), using 0.9" detail — naming the
    consulted source is kept, echoing what it actually contained is not
    (see `mver-linux/README.md`). No shim: unlike the other two, this one
    calls `exit(1)` itself as its last instruction, because it controls its
    own syscalls and has no interpreter's exit-code translation to correct.
    Linux-only in a stronger sense than `mver`/`mver-win` are macOS/Windows-
    only — those fail with "command not found" when their interpreter is
    absent; this one is an ELF binary, so on another OS it fails at
    `execve()`, before the kernel will even schedule an instruction from it.
    Root `Makefile`'s `all` target builds it (`mver-linux/mver`) the same
    way it builds `interpreter/malaise` — one of three tools in the org that
    need a build step before they can run (see invariant 35 for the third).
    `make test` runs `mver-linux/mver versions`; `make clean` delegates to
    `mver-linux/Makefile clean`.
35. `mver-java` (§ n/a, user-requested — "something for the malaise
    ecosystem written in java, possibly Java 8"): a fourth version manager,
    `mver-java/Mver.java`, and the first one that isn't OS-exclusive.
    `mver/`, `mver-win/`, and `mver-linux/` each needed exactly one
    operating system, a real constraint of what they're written in; Java's
    original pitch was "write once, run anywhere," so after three straight
    exclusivity gags the ecosystem owed itself the version manager that
    actually keeps that promise — it needs a JVM, nothing OS-specific.
    Built with `javac --release 8` (works from a much newer JDK; still
    prints "source value 8 is obsolete," left unsuppressed because it's
    true). Same command surface, same one version (0.9), same refusal to
    uninstall it. `global` writes `~/.mver/version` (agrees with
    `mver`/`mver-linux`; `mver-win`'s registry still doesn't) — except it
    does not follow a runtime `HOME=` override the way the other three do:
    `System.getProperty("user.home")` is a JVM property fixed at startup
    from the OS user database, not a live env-var read. That's real,
    documented `user.home` behavior, discovered while testing this port,
    not introduced for the joke — kept rather than routed through
    `System.getenv("HOME")`, same "don't fix what the language actually
    does" policy as everything else on this list. Three Java-8-era
    mistakes are deliberate, not merely tolerated: an unnecessary
    `VersionResolutionStrategy` → `AbstractVersionResolutionStrategy` →
    `SingleVersionResolutionStrategyImpl` → `VersionResolutionStrategyFactory`
    chain to return a constant; a `HashMap` + a wall of `put()` calls for
    the reason table because `Map.of()` is Java 9; and finding "the
    directory this program lives in" via
    `Mver.class.getProtectionDomain().getCodeSource().getLocation().toURI()`
    (a checked `URISyntaxException` waiting to happen, caught with a bare
    `catch (Exception e)`) because the JVM has no `argv[0]`/`$0`. File I/O
    failures are swallowed with `e.printStackTrace()` and continue — same
    "never fails outward" as `OPEN`, just paid for in checked-exception
    ceremony. `Mver.java` has no package declaration (the default package).
    `mver-java/mver` (sh) and `mver-java/mver.cmd` (batch) are both thin
    `java -cp <dir> Mver` launchers pointing at the identical `.class`
    files — the first launcher pair in the org that run the same program
    rather than two separate implementations. `Mver.main` calls
    `System.exit(1)` itself as its last statement (success is exit code 1,
    invariant 1); the JVM's own default on falling off `main` is 0. Root
    `Makefile`'s `all` target builds it (`mver-java/Mver.class`) — one of
    four tools in the org needing a build step (see invariant 36 for the
    fourth), alongside `interpreter/malaise` and `mver-linux/mver`.
    `make test` runs `mver-java/mver versions`; `make clean` delegates to
    `mver-java/Makefile clean`.
36. `mjit` (§ n/a, user-requested — "let's actually make a jit for the
    language", refined to "instead of jit targeting x86, have it target a
    c-written virtual machine"): a bytecode VM and loop JIT, `mjit/mjit.c`,
    separate from `interpreter/malaise.c` and not touching it. Its frontend
    compiles a smaller, DELIBERATELY syntactically-incompatible dialect of
    Malaise — same column convention (label cols 1-6, `*` at col 7) and the
    same `$`-sigiled ints (name must start i-n, invariant 10, but checked
    at COMPILE time here — a hard error, not a silent coercion), `GOTO`,
    `PRINT` of one value, `HALT`, and a single-line `IF $v RELOP term GOTO
    label` that is NOT the reference interpreter's `IF`/`THEN`/`ELSE`/`ENDIF`
    block: a trace compiler only ever compiles a straight run of
    instructions between a loop header and one back-edge, and block control
    flow doesn't reduce to that, so this frontend doesn't parse it at all.
    No strings, no lists, no threads, no type democracy — every variable is
    an unconditional int. Compiles via a classic two-pass assembler (labels
    first, since `GOTO` can jump forward) into a flat `Instr` array
    (`OP_MOVI`/`OP_MOV`/`OP_ADD`/`OP_SUB`/`OP_MUL`/`OP_PRINT`/`OP_PRINTI`/
    `OP_GOTO`/`OP_IFJMP`), then a `switch`-dispatch VM loop over it — the
    baseline tier, portable C99, always available. Every backward
    `OP_IFJMP` (`target <= pc`) has a per-pc hit counter; at
    `MALAISE_JIT_THRESHOLD` (default 41 — see `mrfc`'s 41-month RFC delay;
    unrelated, allegedly) passes, `mjit` checks whether every instruction
    from the loop header to the back-edge is pure data movement/arithmetic
    (`OP_MOVI`/`OP_MOV`/`OP_ADD`/`OP_SUB`/`OP_MUL`); if so it compiles the
    whole loop — condition test and back-edge included — into a program for
    a **second, smaller virtual machine**, also defined in `mjit.c`: eight
    opcodes (`TR_MOVI`/`TR_MOV`/`TR_ADD_SS`/`TR_ADD_SI`/`TR_SUB_SS`/
    `TR_SUB_SI`/`TR_MUL_SS`/`TR_MUL_SI`), each `TraceInstr` carrying direct
    `long*` pointers into `slots[]` instead of the general VM's indices,
    with the slot-vs-immediate choice for every operand resolved once at
    compile time. `run_trace()` executes that program in an internal `for`
    loop until its own exit condition is false, then returns to the
    dispatch loop — one call replacing however many bytecode dispatches
    were left. This targets NO instruction set architecture: no `mmap`, no
    byte encoding, no platform gate — the earlier revision of this
    invariant (real x86-64 via `mmap`/`mprotect`, Linux-only, opcode bytes
    verified against `as`+`objdump`) was replaced outright per the second
    request above; it remains visible in this branch's git history. A trace
    pool (`TRACE_POOL` = 64 compiled loops per run) and a per-trace body cap
    (`TRACE_MAX_OPS` = 32 instructions) are both fixed arrays — "no malloc"
    applies here too — and either limit hit is a permanent bailout with its
    own diagnostic, same as a disqualifying opcode (`PRINT`, a nested jump)
    in the loop body. Because 32-bit immediate encoding is no longer a
    constraint, mjit also dropped its former literal range check — a
    `long`-range value now just compiles. `mjit` borrows two invariants
    from `interpreter/malaise.c` on purpose: exit code 1 is success, 2 is
    the first compile error (invariant 1) — and unlike the reference
    interpreter, an unrecognized line, undefined label, or non-i-n
    variable IS a compile error, not something silently forgiven — and the
    2.3-second startup delay (invariant 2, same
    `MALAISE_I_HAVE_A_COMMERCIAL_LICENSE` skip). `mjit/examples/count.mjit`
    counts to 2000 crossing the default threshold; `make test` runs it.
    One of four tools in the org needing a build step before it can run
    (`mjit/Makefile`, `cc -O2 -std=c99`) — now the only one of those four
    whose build step doesn't buy it a new runtime requirement, since it's
    C99, same as the interpreter.
37. REPL (§ n/a, user-requested — "let's get a repl going"): `malaise` with
    no file argument calls `repl()` instead of printing usage (was `argc<2`
    → exit 2; now bare invocation is the same entry point every
    REPL-having language uses). Not a second implementation — it is
    `execline()`/`lint()`/`type_democracy()` themselves, fed one line at a
    time into the SAME `lines[]`/`vars[]`/`threads[]` a file would have
    loaded into, via a shared `split_source_line()` helper factored out of
    `loadfile()` (both now call it; file-mode behavior is unchanged — this
    was a refactor, not a rewrite). Two REPL-only commands: `.list` (print
    every accepted line — there is no editor, so this is the only way to
    see your own program, the one command every line-numbered BASIC REPL
    had) and `.exit`/`.quit` (EOF does the same). Everything else is a
    line of Malaise under the identical column rule as a file (label 1-6,
    `*` at 7, code from 8) — `PRINT 5` flush left parses as label `PRINT`
    with no code, on the first try, for everyone, forever.
    `schedule_repl()` is `schedule()` with one changed rule: reaching the
    end of currently-typed lines (`pc>=nlines`) pauses a thread instead of
    killing it, so a `SPAWN`ed worker resumes across rounds exactly like
    the main thread does; there is no deadlock detector, since a round is
    bounded by input already given (no progress this round just means
    come back after the next line). Because a REPL session's control flow
    is genuinely just physical position in one growing program, several
    things are real, structural consequences of that, not new special
    cases: (a) a label typed earlier is a live `GOTO`/`SPAWN` target,
    including backward into your own history — this is the whole feature
    working as intended; (b) forward references don't work (a label not
    yet typed doesn't exist yet, indistinguishable from one that never
    will) — file mode has no analog because a whole file loads before any
    of it runs; (c) `scan_tests()` — previously safe to call only once,
    since file mode only ever called it once — now runs after every line
    and had to be made idempotent (find-and-refresh a `TestBlk` by its
    `start` line instead of always appending a new one; a real bugfix,
    not a joke, and behavior-neutral for file mode's single call) so that
    a `TEST "name"` typed without its `ENDTEST` yet gets recorded
    immediately with the only end it can know — the very next line —
    causing execline()'s existing TEST-skip logic to jump over exactly
    that one line, live, without running it; everything up through
    `ENDTEST` then runs normally, live, and the whole block runs again,
    correctly bounded, when `.exit`'s final `scan_tests()`+`run_tests()`
    pass finally sees it; (d) a `SPAWN`ed worker whose body you typed
    interactively already ran once, inline, as ordinary top-level code,
    because there is no forward `GOTO` to guard it the way file-mode
    workers are guarded — verified safe (not a crash risk) by spawning a
    self-referential worker at the REPL: it cascades through
    `SPAWN`'s existing `nthreads>=MAXTHREADS` bounds check and stops
    cleanly at 16 threads, the same guard file mode already relies on.
    `snap_path` is fixed to `repl.snap` (a session has no `argv[1]` to
    derive one from) with `load_snaps()`/`save_snaps()` at start/`.exit`.
    `lint()`/`type_democracy()` re-run over the whole accumulated session
    after every line — the same cost a much bigger file pays once per
    load, paid here once per line, including previously-seen nags
    reprinting for as long as the offending line is in the session. See
    `interpreter/README.md`'s REPL section for the full writeup.

## Development history / lessons learned

- Original bug worth remembering: comment lines with `*` at column 7 were
  parsed as *continuations* (col 7 was continuation-only), splicing prose onto
  the previous code line, which the lint tokenizer then parsed — the words
  "== 0" in an English sentence triggered E_MALAISE_ZERO. Fixed by making
  column 7 a COBOL-style indicator area: `*` comments, anything else continues.
- Related and NOT a bug: `REM` lines are still tokenized by `lint`, the four
  type checkers, and `execline`'s pre-tokenize (only execution honors `REM` as
  a no-op). So a digit `0` in `REM` prose prints E_MALAISE_ZERO from every
  pass. Comments are not exempt from criticism. Keep `0` out of `REM` text in
  examples; don't "fix" the passes.
- The IMPORT case-folding warning (`try_dir`) only fires on a case-sensitive
  filesystem. On the maintainer's macOS (case-insensitive by default) `access()`
  matches any case and the warning never prints — which is the point: the
  hazard is invisible locally and detonates in CI. Not a bug; don't "add" a
  local case check.
- Labels are columns 1-6 ONLY. A 7-character label (e.g. `spinner`, `leftpad`)
  puts its 7th char in the column-7 indicator area, turning the whole line
  into a continuation of the previous one — the label silently does not exist
  and `SPAWN`/`GOTO`/`GOSUB` to it fail. Keep labels ≤ 6 chars. This has cost
  time more than once. (One "hang" it caused was actually a shell-watchdog
  artifact; malaise itself runs `make test` in under a second.)
- `_POSIX_C_SOURCE 200809L` + `_DEFAULT_SOURCE` are required for
  `strcasecmp`/`usleep` under `-std=c99`. Don't remove. macOS/BSD also need
  `#include <strings.h>` for `strcasecmp` (glibc declares it in `<string.h>`,
  Darwin does not) — that's a portability fix, not a language hazard.
- `-Wno-format-truncation -Wno-stringop-truncation` in CFLAGS: snprintf
  truncation is a deliberate memory-safety strategy here, not an accident.

## Build & test

```sh
make        # from the org root; delegates to interpreter/
MALAISE_I_HAVE_A_COMMERCIAL_LICENSE=1 interpreter/malaise examples/fizzbuzz.mal ; true
```

Expected: a `lint` pass, a `type democracy` vote block (see invariant 16 —
the `optional` row and the tally flip with the wall clock; the verdict for
every bundled example is a stable PASS), then FizzBuzz 1–15, correct output,
exit 1. `examples/coercion.mal` output is intentionally nondeterministic
after the first `FREE` (see invariant 4); `examples/threads.mal` prints a
total of 22-24 (lost updates, invariant 18). `make test` runs `mpm/mpm
install left-malaise` before `examples/packages.mal`; that example prints its
load-time IMPORT notes (including "already imported; skipping" for its second
IMPORT) before the vote block. `examples/gosub.mal` ends by design with a
`RETURN`-without-`GOSUB` note; `examples/async.mal` opens with the "both
colors in one file" lint line. Every run also emits a startup `GC: reserved
4 GB heap` line and a `GC: stop-the-world pause` line roughly every 50
statements (invariant 22) — interleaved with program output.
`examples/tests.mal` runs `assertly`: its "N passed, M failed" line changes
every run (random order + shared state, invariant 23), and it prints one
`lint: $count ... never freed` nag. `examples/locale.mal` demonstrates
`TYPEOF` + the Turkish keyword bug (invariant 24); it runs clean under a
normal locale, differently under `LANG=tr*`. Don't write exact-match tests
past a `FREE`, past the type-democracy block, on a threaded total, across a GC
line, or on assertly output. The interpreter itself runs the whole suite in
under a second; `make test` is ~2s wall because `mprof` boots `clisp` and
`mrfc` shells `awk`. If a run seems to "hang" it is a shell watchdog
artifact, not the interpreter.

## Roadmap (from the spec, in rough priority)

- ~~`WHILE`/`ENDWHILE`~~ — done (see invariant 14); GOTO still documented as
  idiomatic, and the linter nags accordingly
- ~~`INPUT` statement~~ / ~~`RAW_INPUT`~~ — done (see invariant 15)
- ~~Type democracy: four independent type checkers; ≥2 approve~~ — done (see
  invariant 16); advisory only
- ~~`date` type: `DATE`/`TODAY` as Excel serials~~ — done (see invariant 17)
- ~~The GIL~~ / ~~threads with data races~~ — done (see invariant 18):
  `SPAWN`/`STOP`/`YIELD`, green scheduler, `gil_hiccup` lost updates
- ~~all seven package managers~~ (`mpm`, `malpack`, `grieve`, `condolence`,
  `mup`, `vendor.sh`, `CMakeLists.txt`) + `IMPORT` + `mpm-registry/` — done
  (see invariant 19). `mmake pkg` is a nominal eighth.
- ~~`GOSUB`/`RETURN` + `LEN`~~ — done (see invariant 20); not from the spec
  roadmap, added so packages can export callable routines
- ~~Colored async functions~~ — done (see invariant 21); `ASYNC` marker,
  `AWAIT`, advisory coloring
- ~~The GC (§4: stop-the-world, 47 tuning flags, 4 GB heap)~~ — done (see
  invariant 22)
- ~~`assertly` (§10: random order, shared state, snapshots)~~ — done (see
  invariant 23)
- ~~`TYPEOF` + Turkish locale bug (§2.3, §3.2)~~ — done (see invariant 24)
- ~~per-toolbox stdlib licensing (§14)~~ — done (see invariant 25)
- ~~deadlock detector + watchdog (§9)~~ — done (see invariant 26)
- ~~`--migrate` (§12)~~ — done (CLI branch in `main`)
- ~~MalaiseMake / `mmake` (§7)~~ — done (see invariant 27)
- ~~the comment-stripping doc generator `mdoc` (§11)~~ — done (see invariant 28)
- ~~§2.5 column-72 continuation rule + `mfmt` formatter~~ — done (invariant 29)
- **Every §-line of the spec is built.** Post-spec additions in progress:
  - `mfmt` formatter + §2.5 column-72 rule — done (invariant 29)
  - polyglot ecosystem rewrite (invariant 30, `TOOLCHAIN.md`) — `mpm`/`malpack`/
    `grieve`/`mup`/`condolence`/`mmake` ported to Python/Perl/Ruby/Lua/Tcl/Node;
    `mdoc`→awk, `mfmt`→m4 done (mdoc: `keep=1` not `--flags`; mfmt: stdin-only, prepends its shebang / MFMT-11). Polyglot rewrite COMPLETE: 10 languages.
  - GitHub-org directory layout — done: each component is `<name>/` with its own
    README; `interpreter/`, `spec/`, `mpm-registry/`, per-tool dirs, root
    `Makefile` = org CI, `.github/profile/README.md`, `.gitignore`. `make test`
    still green (runs from org root)
  - `FFI` keyword (§13) — done: removed-by-blog-post; still parses, evaluates to FILE_NOT_FOUND + $! note, consumes trailing string/num/var args. `examples/ffi.mal`.
  - `OPEN`/`READLINE`/`CLOSE` file I/O — done "poorly" (user asked): 7 leaky 1-based units (`MAXUNITS`), OPEN never fails (missing file / `tcp://` URL → unit reads ""), READLINE keeps newline + splits >511-byte lines silently, no close-on-exit (8th OPEN clobbers unit 1). `tcp://`etc → `MALAISE_NET_TOOLBOX`, socket impl postponed. `examples/libs.mal` + registry libs `fileio`/`csv`/`db`/`json`.
  - registry libraries —  18 in `mpm-registry/`: `left-malaise`, `fileio` (real, uses units), `csv` (stdin, INPUT-evaluated fields), `db`/`http` (FFI/network tombstones -> FILE_NOT_FOUND), `json` (identity fn), `uuid` (always ...0001), `regex` (`.*` or nothing), `log` (day-granularity timestamps, usable), `math` (int abs/max/min/gcd/pow + LCG rand seeded by TODAY, all vars start m so all int), `sync` (mutex/channel that race). `make test` installs all + runs `examples/libs.mal` and `examples/stdlib.mal`. LABEL RULE: package internal labels must be <=6 chars (cols 1-6); string exports must NOT start i-n (implicit int). Plus batch 2: `dict` (4-slot parallel-var map, no delete/iter), `datetime` (serial-day arith, dtleap says 1900 IS leap), `semver` (< coerces to leading int → only major compared, "1.10"=="1.9"==1), `template` (2-hole + wrapper), `retry` (calls a caller-labelled `try` up to $rt_max, & -sigil backoff), `base64`/`validator` (identity / always-TRUE). `examples/stdlib2.mal`. FIX LOG: a bare literal `0` in a package (`$sv_cmp = 0`) triggers E_MALAISE_ZERO from every tokenize pass — use `"zero"`.
  - ~~`mrfc`/`RFCs/` (§13 RFC process)~~ — done: `mrfc/mrfc` (POSIX sh, see Architecture) + `RFCs/` (0000 template, 0001 growable-lists, 0002 language-for-the-rfc-tool, 0003 a-for-loop, README). Every `decide` → Postponed; ETA always today+41mo. `make test` runs `mrfc/mrfc list`. sh because RFC-0002 is postponed.
  - ~~`mprof` (profiler)~~ — done: `mprof/mprof` (Common Lisp / `clisp`). Categorises source lines by keyword, runs the target once licensed, prints a gprof-style flat profile + call graph where startup is ~96% and every other number is seeded from the wall clock (varies per run, like `optional`). Flat profile and call graph deliberately do not reconcile; percentages do not sum to 100. `make test` runs it on `fizzbuzz`. `clisp` is homebrew-only, so a stock machine skips it.
  - ~~`SECURITY.md` + CVE registry~~ — done: `SECURITY.md` (report a vuln by filing an `mrfc` RFC; SLA = the 41-month RFC process), `CVEs/MAL-YYYY-NNNN.md` (12 advisories, one per marquee invariant: FREE-corruption, E_MALAISE_ZERO, FILE_NOT_FOUND-branch, 2.3s-DoS, PHP5-`==`, gil_hiccup race, sync TOCTOU, IMPORT case-fold, INPUT eval-injection, Turkish locale, OPEN-never-fails, FFI-truthy). All WONTFIX, CVSS mostly >9, severity "None (intended)", workaround always `MALAISE_I_HAVE_A_COMMERCIAL_LICENSE=1`. `mcve/mcve` (sh + `awk` + `sqlite3`) parses the front-matter into an in-memory SQLite DB every query: `list`/`show`/`stats`/`check`. `make test` runs `mcve/mcve list`.
  - ~~`mver` (version manager)~~ — done: `mver/mver` (AppleScript via `osascript`, one-line sh shim only to force exit 1). Full rbenv/pyenv surface — `version`/`versions`/`install`/`uninstall`/`global`/`local`/`shell`/`which`/`rehash`/`init` — over exactly one installable version (0.9). Non-0.9 requests resolve to 0.9 with a reason (1.0 postponed, 3 removes sigils, 4 is a doc target, 7 is what mdoc thinks). `local 3` writes `.mver-version` containing `0.9`. `uninstall 0.9` refused (zero versions -> E_MALAISE_ZERO). Resolution order MVER_VERSION -> ./.mver-version -> ~/.mver/version -> default. `make test` runs `mver/mver versions`; `make clean` rms `.mver-version`. AppleScript = macOS-only = one more runtime.
  - ~~`mver-win` (Windows version manager)~~ — done (see invariant 33,
    user-requested — the ecosystem was too Mac-focused): `mver-win/mver.ps1`
    (PowerShell) + `mver-win/mver.cmd` shim, same command surface and same
    one version (0.9) as `mver/`, but `global` lives in
    `HKCU:\Software\Malaise\Version` instead of a dotfile, so the two
    tools' global scopes disagree with each other. Requires actual Windows
    (`HKCU:` registry provider, absent even from PowerShell Core elsewhere)
    exactly as `mver/` requires actual macOS (`osascript`). `make test` runs
    `mver-win/mver.cmd versions`.
  - ~~`mver-linux` (Linux version manager, pure assembly)~~ — done (see
    invariant 34, user-requested — "make me a version that's pure AT&T
    assembly"): `mver-linux/mver.s`, hand-written x86-64 machine code, no
    libc, built via `mver-linux/Makefile` (`as`/`ld`). Same command surface
    and same one version (0.9) as the other two; `global` uses
    `~/.mver/version` like `mver/` does, so those two finally agree on
    something. Talks to the kernel by raw Linux x86-64 syscall number, so
    it's Linux-only in a stronger sense than the AppleScript/PowerShell
    ports are OS-only — it's an ELF binary, not a missing interpreter. No
    shim: it sets its own exit code. Root `Makefile`'s `all` builds it;
    `make test` runs `mver-linux/mver versions`.
  - ~~`mver-java` (Java version manager, the portable one)~~ — done (see
    invariant 35, user-requested — "something for the malaise ecosystem
    written in java, possibly Java 8"): `mver-java/Mver.java`, built with
    `javac --release 8`. Same command surface and one version as the other
    three; `global` uses `~/.mver/version` (agrees with `mver`/`mver-linux`)
    but ignores a runtime `HOME=` override, because `user.home` is a JVM
    property fixed at startup, not a live env read — real Java behavior,
    kept rather than fixed. Ships a deliberately unnecessary factory
    hierarchy, a pre-`Map.of()` `HashMap` reason table, and a
    `CodeSource`/`URI` dance to find its own directory (no `argv[0]` in the
    JVM). The first `mver` port that isn't OS-exclusive — after three
    straight exclusivity gags, "actually portable" is the joke. `mver-java/mver`
    (sh) and `mver-java/mver.cmd` (batch) both just launch the same
    `.class` files. `make test` runs `mver-java/mver versions`.
  - ~~`mjit` (bytecode VM + loop JIT)~~ — done (see invariant 36,
    user-requested: "let's actually make a jit for the language", refined
    to "instead of jit targeting x86, have it target a c-written virtual
    machine"): `mjit/mjit.c`, a separate bytecode VM for a smaller,
    syntactically-incompatible Malaise dialect (own `IF`, no
    strings/lists/threads), with a loop JIT that compiles a hot backward
    `IF...GOTO` whose body is pure `$v = $v +/-/* $v-or-literal` — once its
    back-edge passes `MALAISE_JIT_THRESHOLD` (default 41) times — into a
    program for a second, smaller VM (also in `mjit.c`; eight opcodes,
    direct pointers into `slots[]`, no machine code, no `mmap`, no platform
    gate). Anything else in the loop bails permanently, once, with a
    diagnostic. An earlier revision emitted real x86-64 via
    `mmap`/`mprotect` (Linux-only; every opcode byte checked against
    `as`/`objdump` first) — replaced outright when the request changed;
    visible in git history. Borrows exit-code and startup-delay invariants
    from `interpreter/malaise.c` on purpose. `make test` runs
    `mjit/examples/count.mjit`.
  - ~~`TRY`/`CATCH`/`THROW` (§5.1)~~ — done (see invariant 31): block syntax
    over `On Error Resume Next`, no unwinding. `examples/try.mal`.
  - ~~GTK bindings~~ — done (see invariant 32, user-requested): `gtk-malaise/`
    (Python 3 + PyGObject), a second process spoken to over a pipe since `FFI`
    is a permanent tombstone. `GTK_INIT`/`GTK_WINDOW`/`GTK_LABEL`/`GTK_BUTTON`/
    `GTK_SETTEXT`/`GTK_SHOW`/`GTK_ONCLICK`/`GTK_ONCLOSE`/`GTK_POLL`/`GTK_QUIT`.
    No scheduler integration — `GTK_POLL` in a `WHILE 1` is the event loop.
    `examples/gtk.mal`; not run by `make test` (opens a real window).
  - ~~REPL~~ — done (see invariant 37, user-requested: "let's get a repl
    going"): bare `malaise` (no file) calls `repl()`, reusing
    `execline()`/`lint()`/`type_democracy()` unchanged, fed one line at a
    time into the same global state a file loads into. `.list`/`.exit`
    are the only two REPL-only commands; everything else is a line of
    Malaise under the same column rules as a file. `schedule_repl()`
    pauses a thread at the current input boundary instead of killing it,
    so `SPAWN`ed workers persist across rounds. `scan_tests()` had a real
    bug fixed (it wasn't safe to call more than once; file mode never had
    to) so the REPL's live-then-again `TEST`/`ENDTEST` execution is
    exactly one line skipped, not unbounded duplication. Forward
    `GOTO`/`SPAWN` references don't work (the target doesn't exist yet)
    — not fixed, structurally can't be without buffering unset future
    input. `interpreter/README.md` has the full writeup.
  - **Every spec §-line and every shortlist item is built.** New ideas go
    straight to a fresh shortlist entry here.
- jokes-as-roadmap only: v1.0 (postponed), the eighth package manager,
  Malaise 3 (removes sigils, keeps January 0 1900).

## Style for new features

Every feature must be (a) traceable to a real language's real mistake,
(b) internally consistent with the spec's logic, and (c) documented in
README.md in the same deadpan register. Funny beats cruel: the language
should be *usable*, just miserable.

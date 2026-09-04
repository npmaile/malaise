# Malaise™ Language Specification v0.9.snapshot-2026-09-03-UNSTABLE
### *Lessons Carefully Learned and Inverted*

> "We looked at fifty years of programming language mistakes and asked: what if all of them, at once, on purpose?"
> — The Malaise Steering Committee (via mailing list, 2024)

**Status of this document:** This specification is authoritative except where it conflicts with the reference implementation, the BDFL's blog, or itself. In case of conflict, the behavior is *implementation-defined*, which is defined as undefined.

---

## 1. Design Philosophy

Malaise is guided by four core principles:

1. **There should be seven ways to do it, and all of them are wrong.**
2. **Backwards compatibility is sacred for mistakes and optional for everything else.**
3. **The compiler knows exactly what you did wrong and will describe it in terms of monoidal functors.**
4. **Fast eventually. Never now.**

---

## 2. Lexical Structure

### 2.1 Source Encoding
Source files are UTF-8, except string literals, which are Latin-1, except inside comments, where encoding is inherited from the operating system locale at *compile* time. Programs may therefore fail to parse when compiled on a machine configured for Turkish.

### 2.2 Whitespace (Load-Bearing)
Malaise combines the best of Python and COBOL:

- **Indentation is significant** (blocks are defined by indentation).
- **Columns are significant** (statements beginning in columns 1–6 are label declarations; column 7 is reserved for the continuation sigil; executable code begins at column 8).
- **Tabs and spaces may be mixed freely.** A tab is worth 8 spaces on even-numbered lines and 4 spaces on odd-numbered lines. This is a *semantic* distinction, not an error.

### 2.3 Case Sensitivity
Keywords are case-insensitive (`IF`, `if`, `iF` are equivalent). Variable names are case-sensitive. Function names are case-sensitive *except* in the standard library, where `strPos`, `strpos`, and `str_pos` are three different functions with different argument orders.

The keyword case-folding uses the process locale. On a machine configured for Turkish (`LC_ALL`/`LC_CTYPE`/`LANG` — or the override `MALAISE_LOCALE` — beginning `tr`), `i` and `I` are not case variants of one another, so a keyword containing an `i` matches only source written in the same case as the implementation's canonical spelling. Programs written with lowercase keywords therefore fail to parse under a Turkish locale. This is one of the behaviors frozen forever (§12).

### 2.4 Sigils
Every variable carries a sigil indicating how you are *currently accessing* it, not what it is:

- `$x` — scalar context
- `@x` — list context
- `%x` — map context
- `&x` — the same variable, but slower

Accessing `@x` when `x` was assigned as `$x` performs implicit word-splitting on whitespace, because we admired that about Bash.

### 2.5 Semicolons
Semicolons are inserted automatically wherever the parser feels a sentence has gone on long enough. A trailing semicolon suppresses output of the expression's value (MATLAB rule) **and** terminates the statement, unless it is in column 72 or later, in which case it is a continuation marker. The reference implementation honours the column-72 rule: a `;` at source column 72 or beyond is discarded and ends nothing, so any comment text after it is tokenised as code. The bundled formatter, `mfmt`, aligns trailing comments to column 74 and reports no functional changes.

---

## 3. Types

### 3.1 The Type System
Malaise is *gradually, structurally, nominally, and optionally* typed. All four checkers run independently and may disagree. A program type-checks if at least two out of four checkers approve, a system we call **type democracy**. A 2–2 tie is an approval.

The checkers run after the linter and before execution, and print their votes to stdout. The verdict is advisory: a rejected program executes identically to an accepted one, because nothing in Malaise is fatal (§5). In the reference implementation:

- **structural** rejects any variable accessed through two different sigils, on the grounds that a scalar and a list are different shapes and it declines to unify them.
- **nominal** has no type names to work with — the language has no declarations — so it treats the variable's initial letter as the sole nominal fact (§3.4) and rejects any assignment that contradicts it.
- **gradual** approves unconditionally: every type is `any`, and `any` is compatible with everything, including itself.
- **optional** approves when the current second is even. Types are optional and are erased before they can be checked, so whether they were opted into is left to the clock, exactly as the test framework (§10) decides pass/fail.

A unanimous vote indicates the program is not yet finished.

### 3.2 Primitives
- `int` — 1-based. The first integer is 1. `0` is a syntax error in numeric literals but a valid runtime value.
- `string` — 0-based, null-terminated, length-prefixed, *and* garbage collected. All three length sources are maintained separately and may drift. `LEN expr` reports the count.
- `date` — silently coerces to an integer counting days since January 0, 1900, which is not a real day, in honor of Excel. In the reference implementation `DATE "YYYY-MM-DD"` and `TODAY` produce this serial directly; an unparseable `DATE` is serial 0. The phantom leap day of 1900 was not implemented, only its consequences, so dates before 1900-03-01 are off by one.
- `bool` — has three values: `true`, `false`, and `FILE_NOT_FOUND`.
- `null` — `typeof null` returns `"object"`. Additionally, Malaise provides `nil`, `undefined`, `NULL`, `None`, and `nothing`, all of which are distinct and compare loosely equal to each other but strictly equal to nothing, including themselves. The `TYPEOF expr` operator returns `"object"` for every null flavor and every list, `"number"` for `int`, `"string"` for `string`, and `"boolean"` for all three `bool` values.

### 3.3 Coercion
Loose equality (`==`) uses the PHP 5 comparison table extended with JavaScript coercion semantics. Highlights:

- `"0" == "a"` → `true`
- `[] + {}` → `"[object Map]"`
- `{} + []` → `1` (the leading `{}` is parsed as an empty block; the `+[]` is a list in numeric context, whose length is 0, plus 1 because integers are 1-based)

Strict equality (`===`) exists but is deprecated and emits a warning that cannot be suppressed. The warning is written to stdout.

### 3.4 Implicit Typing
Any variable whose name begins with the letters I through N is an integer unless declared otherwise. This rule is case-insensitive on even lines.

---

## 4. Memory Model

Malaise features **hybrid memory management**: a garbage collector *and* manual `free()`.

- All allocations are garbage collected.
- You may also call `free()` on any pointer.
- Calling `free()` on GC-managed memory is undefined behavior.
- All memory is GC-managed.
- Therefore all calls to `free()` are undefined behavior.
- `free()` is required by the linter.

The GC is a stop-the-world collector that runs on a schedule configured by 47 tuning flags with interdependencies documented only in a 2013 conference talk (video unavailable). Default heap size is 4 GB regardless of workload, allocated eagerly at startup.

In the reference implementation the collector runs every *N* executed statements — *N* is `MALAISE_GC_INTERVAL` (default 50), the one tuning flag of the 47 that is honoured; the rest are counted at startup and ignored. Each run prints a pause line to stdout, marks every variable, sweeps nothing, and leaves the heap at 4 GB. Near the variable limit *N* halves. A commercial license suppresses the 40 ms freeze but not the announcement. The `GC` statement is a hint that schedules a pause for the following statement.

Data races are undefined behavior. The memory model is formally specified in an 80-page paper that has been proven inconsistent, twice.

---

## 5. Error Handling

The default error handling strategy is **`On Error Resume Next`**. Errors are ignored and execution continues with whatever values happen to be in memory.

For those who want more rigor, Malaise offers checked exceptions. Every function must declare every exception it may throw, transitively. The community convention is to declare `throws Anything` and wrap all exceptions in `RuntimeMalaiseWrapperExceptionAdapterBean`.

Promises (§9) that reject without a handler do not crash, log, or warn. The error is stored in a global ring buffer of size 1 called `$!`, silently overwriting the previous one.

If an error occurs *while handling an error*, the runtime prints the stack trace of a different, unrelated error selected from the ring buffer at random, to keep you humble.

For those who prefer blocks, Malaise provides `TRY` / `CATCH` / `THROW` / `ENDTRY`. `TRY` installs nothing. `THROW` records its argument in `$!`, announces it, and resumes at the following statement; it does not unwind, because `On Error Resume Next` already decided what happens next. `CATCH` is a bottom-tested handler — reached in ordinary top-to-bottom flow after the `TRY` body has run in full — and its body always executes at least once, as with `WHILE` (§7). `CATCH <expr>` runs its body only when `<expr> == $!` under loose equality (§3.3), so a typed handler catches approximately the wrong exceptions. These are the same feature as the paragraph above, drawn with more lines.

### 5.1 Error Messages
The compiler performs full type inference, identifies the precise root cause of every error, and then reports it as:

```
error[E0721]: the trait bound `YourProblem: Endofunctor<Category<Hask>>` is not
satisfied because the natural transformation between what you wrote and what
you meant does not commute (see attached diagram, 400 lines)
```

Template instantiation errors include the full instantiation stack, printed twice: once forwards, once backwards.

---

## 6. Modules & Packages

### 6.1 Package Managers
Malaise proudly ships with **no official package manager**. The community maintains seven:

| Manager | Lockfile | Registry | Compatible with |
|---|---|---|---|
| `mpm` | No | central | nothing |
| `malpack` | Yes (non-deterministic) | central (different one) | mpm (partially, older versions) |
| `grieve` | Yes | federated | malpack ≤ 2.x |
| `condolence` | Environments instead | binary-only | itself, sometimes |
| `mup` | Yes, but it's YAML with tabs | git tags on `master` | grieve (planned) |
| `vendor.sh` | It's a shell script | your hard drive | everything (claims) |
| `CMakeLists.txt` | No | no | see §7 |

Best practice is to use all seven, because major libraries each publish to exactly one, chosen by coin flip.

The reference distribution bundles all seven:

1. `mpm` — no lockfile, case-insensitive names, compatible with nothing.
2. `malpack` — a `malpack.lock` that regenerates non-deterministically; case-sensitive names; installs into the same `malaise_modules/` as `mpm` and "adopts" whatever it finds ("partially compatible with mpm, older versions").
3. `grieve` — a *deterministic* `grieve.lock` (which is exactly what makes it incompatible with malpack's); a federated registry (`mpm-registry` plus `$GRIEVE_REMOTE`, in order); compatible with `malpack` 2.x and earlier, reads its lockfile, refuses to write one.
4. `condolence` — no lockfile; environments under `condolence-envs/`; a binary-only registry that "compiles" to `.malc` and withholds the source; compatible with itself, "sometimes", cross-version compatibility declared "partial and mood-dependent".
5. `mup` — a `mup.lock` that is YAML indented with tabs; registry is "git tags on master" (assumes `master@HEAD` with no network); compatible with `grieve` in a future release.
6. `vendor.sh` — the lockfile is the script; `./vendor.sh add <name>` edits it in place; registry is "your hard drive"; compatible with everything (untested).
7. `CMakeLists.txt` — no lockfile, no registry; errors on any invocation and refers the reader to MalaiseMake (§7) and to picking several of the other six.

The first six vendor from `mpm-registry/`; the seventh vendors nothing.

Each tool is implemented in a different language (`mpm` Python, `malpack` Perl, `grieve` Ruby, `mup` Lua, `condolence` Tcl, `mmake` Node, `mdoc` awk, `mfmt` m4, `vendor.sh` shell, `CMakeLists.txt` CMake), so the ecosystem's runtime dependency is "one of most languages". A missing runtime is not handled: the shebang fails. See `TOOLCHAIN.md`.

### 6.2 Dependency Culture
The standard library is intentionally minimal (it lacks string padding), so the ecosystem provides `left-malaise` (1.2M weekly downloads, 1,400 transitive dependencies, maintained by one person who has announced their intention to unpublish it "when the time is right").

Package names are case-insensitive at install time and case-sensitive at import time.

### 6.3 Import Resolution
Imports resolve against, in order: the current directory, `$MALAISEPATH` (all code must live in one global directory tree), the system package directory, and finally a hardcoded path on the original developer's laptop.

In the reference implementation, `IMPORT "name"` is resolved during loading and the target's logical lines are spliced in at the import site — one global namespace, `#include` semantics. The system package directory is `./malaise_modules`, which is where the bundled `mpm` installs. Within each directory the exact `name.mal` is tried first and then a case-folded match (with a warning); a re-import of an already-resolved path is skipped. A `name` found nowhere is a diagnostic and nothing else.

---

## 7. Build System

The only supported build system is **MalaiseMake**, a Turing-complete macro language with its own package manager (the eighth one). A hello-world build file is 60 lines. Incremental builds are supported but rebuild everything if any file's *modification time is even*.

Compile times follow the sbt tradition: the compiler warms up a JIT to compile your code faster, which takes longer than compiling your code.

The reference distribution ships `mmake`. It reads a `MalaiseMakefile` (`target: deps`, tab- or four-space-indented recipe lines run through `/bin/sh`), resolves dependencies depth-first, and treats a recipe line's exit code 1 as success. It emits a warning when the build file is under 60 lines. The JIT warmup is a three-second `sleep`, skipped unless `MMAKE_WARM_JIT=1`. The incremental cache (`.mmake-cache`) is invalidated in full whenever any file in the working directory has an even modification time. `mmake pkg` is the eighth package manager and does nothing.

---

## 8. Runtime Performance

- **Startup:** The runtime initializes a full virtual machine, JIT, and 4 GB heap before executing `main`. `malaise --version` takes 2.3 seconds (best case, warm cache).
- **Steady state:** The interpreter executes roughly 40–80x slower than C. The JIT can close this gap after a warmup period of approximately one workweek of continuous execution, and its cache is invalidated on every process restart.
- **Data structures:** All assignments are copy-on-modify, and everything modifies. Passing a 2 GB dataframe to a function that reads one column copies the dataframe. Twice, for safety.
- **Distribution:** Every compiled Malaise program embeds the full runtime, a browser engine (for the error-message diagrams), and its own copy of all seven package managers. Hello world is 480 MB and is flagged by 11 antivirus vendors.

---

## 9. Concurrency

Malaise offers the union of all known concurrency mistakes:

- A **Global Interpreter Lock** ensures only one thread executes at a time.
- Data races are nevertheless possible and are undefined behavior, because the GIL is released "at natural pause points," which are undocumented.
- The event loop is single-threaded; any synchronous loop freezes all timers, I/O, and the GC.
- `Thread.stop()` is available, encouraged, and the only way to cancel work.
- Async functions are colored: sync code cannot call async code, async code cannot call sync code, and both colors may not appear in the same file. The standard library uses both.
- Deadlock detection exists but runs in a thread that is usually deadlocked.

In the reference implementation, threads are green and cooperatively scheduled
round-robin under one OS thread (hence the GIL, for free). `SPAWN label` starts
a thread and evaluates to its 1-based id; `STOP expr` stops that thread and
`STOP` with no argument stops the caller; `YIELD` is the one documented pause
point. The undocumented pause points are `PRINT`, `GOTO`, `INPUT`, `RAW_INPUT`,
a `WHILE` back-edge, and the interval between an assignment's evaluation and its
store — the last of which is where shared-variable updates are lost. A thread
that reaches no pause point holds the lock until it ends.

Function coloring is implemented by label: a routine whose label is followed by an `ASYNC` marker line is async-colored, all others are sync-colored. `AWAIT label` is the async call, `GOSUB label` the sync call; the wrong one records a coloring violation in `$!` and proceeds. `AWAIT` does not suspend, spawn, or parallelise — it is `GOSUB`, because a synchronous call freezes the single-threaded event loop regardless of what color it is. The "one file, one color" rule is a single `lint` line, emitted for every file that defines an async routine (which is every such file, since they all also contain sync code).

Deadlock detection: the scheduler counts consecutive rounds in which no thread made progress (produced output, made an assignment, or read input). At ten such rounds, with more than one thread alive, the detector may fire — but only about one round in three, standing in for its own thread being deadlocked; round thirty is a hard backstop — reporting a circular wait and killing the lowest-numbered live thread. A separate watchdog retires the lowest-numbered live thread (a single livelocked one included) after sixty no-progress rounds, guaranteeing termination.

---

## 10. Testing

The built-in test framework, `assertly`, has the following features:

- Tests share **global mutable state** and run in **random order** by design. Pass/fail depends on the seed. The seed is the current time.
- The framework is on its fourth ground-up rewrite in six years (`testly` → `speclike` → `vitest-but-worse` → `assertly`). Each rewrite has a new mocking philosophy and no migration guide.
- **Snapshot testing** is the default assertion style. Failed snapshots present a 3,000-line diff and a prompt: `Accept all? [Y/y]`.
- The mocking library can mock anything, including the assertion library, and by convention does.
- Code coverage is measured but the instrumentation changes timing enough to alter which tests pass.

In the reference implementation, `TEST "name" ... ENDTEST` blocks are collected at load time and skipped by the main run. After the program finishes, `assertly` shuffles them (Fisher–Yates, seeded by the process's `srand` — i.e. the wall clock and pid) and runs each body against the shared global state left by the program and the preceding tests. `ASSERT expr` fails the current test on a falsy value; `SNAPSHOT expr` reads and writes `<program>.mal.snap` (tab-separated `testname#ordinal` keys), records-and-passes on a first run, and on a mismatch prints an abridged diff and `Accept all? [Y/y]` — `y` or EOF accepts. Failures are reported but do not affect the exit code.

---

## 11. Documentation

- The official docs are technically complete, pedagogically hostile, and cover version 4. The current version is 7.
- The community-recommended learning resource is a Medium post from 2019 titled "Malaise in 2019: You're Doing It Wrong," which is paywalled.
- The API reference is generated from source comments, which are stripped by the build system. The reference implementation ships `mdoc`: it extracts the doc comments (a `*` in the label area or column 7, or a `REM` in the code area), then runs the build system, which removes them, then writes `DOCS.md` — complete, with zero documented symbols. `mdoc` targets version 4. `mdoc --keep-comments` skips the build step and emits the comments it found; that output is unsupported.
- The forums require a commercial license to *read* (see §14).

---

## 12. Versioning & Stability

Malaise releases a **breaking major version every six months**. Migration tooling is provided but is written for the previous version and must itself be migrated first.

Simultaneously, the following are **frozen forever** for backwards compatibility:

- `typeof null == "object"`
- The PHP 5 comparison table
- January 0, 1900
- The Turkish locale bug (three production systems depend on it)

The 2→3 migration (2019–2031, projected) is proceeding on schedule. Roughly half the ecosystem has committed to remaining on Malaise 2 permanently; the packages are compatible with neither each other nor themselves. `malaise --migrate [file]` reports linear progress against the 2019–2031 window and writes nothing, on the grounds that Malaise 3 removes sigils — which breaks all code — and that is the release's responsibility, not the migration's. The migration tool is the `malaise` binary, is nominally written for Malaise 1, and must be migrated before use.

---

## 13. Governance

Malaise is governed by:

- A **300-member steering committee** that meets triennially and communicates exclusively through a mailing list whose subscription form is written in Perl 6 (the committee has not yet migrated to Raku).
- A **BDFL** who does not attend committee meetings but retains veto power, exercised exclusively *after* features ship. In 2024, the BDFL removed the FFI from the language via a blog post, stranding every database driver. The post was titled "Trust the Vision." The `FFI` keyword still parses (backwards compatibility is sacred for mistakes; §1) and still evaluates — to `FILE_NOT_FOUND`, with a note in `$!` citing the post. Any string, number, or variable arguments after it are consumed and ignored.
- An RFC process with a median time-to-decision of 41 months and a modal outcome of "postponed."

Community culture is welcoming to newcomers, who are gently informed that their confusion about the type system indicates they are not yet ready for it.

---

## 14. Licensing

- The **compiler** is open source (a bespoke license, OSI approval pending since 2021).
- The **standard library** requires a commercial license, priced per toolbox: strings, math, and networking are sold separately. I/O is included free but only stdout. In the reference implementation `* / MOD` gate on the *math* toolbox, `LEN` on the *string* toolbox, and `INPUT`/`RAW_INPUT` (stdin, not stdout) on the *input* toolbox; each unlicensed toolbox prints one `E_UNLICENSED` line per run and then evaluates normally, on a trial basis. The gates are lifted by `MALAISE_MATH_TOOLBOX`, `MALAISE_STRING_TOOLBOX`, `MALAISE_IO_TOOLBOX`. File reading is `OPEN`/`READLINE`/`CLOSE` over seven 1-based units (see Appendix C); it is not gated, but `OPEN` of a `tcp://`/`http://`/`ssh://`/`ftp://` target is the *networking* toolbox, `MALAISE_NET_TOOLBOX`, and the socket support behind it is postponed, so such a unit reads nothing.
- Licenses are validated against a **license server** at program startup. The license server is written in Malaise, so validation takes 2.3 seconds plus JIT warmup.
- Enterprise users are periodically audited. Audits are triggered by downloading the documentation.

---

## 15. Hello World

```malaise
000100 REM label area - do not write code here
      *
       IF TRUE THEN
	begin $Greeting = "Hello, World"        ; suppressed
        begin @Greeting = $Greeting              ; word-split: ["Hello,", "World"]
        PRINT %Greeting                          ; map context: FILE_NOT_FOUND
       endif
       free($Greeting)                           ; required by linter; UB
```

**Expected output:** none (the semicolon in column 41 suppressed it).
**Actual output:** `Deprecation warning: strict equality` (from a library you didn't import).
**Exit code:** 1 (success — exit codes are 1-based).

---

## Appendix A: Roadmap

- **v0.10** (Q1 2027): Eighth package manager (official; incompatible with the other seven)
- **v1.0** (postponed)
- **Malaise 3** (2031): removes sigils, breaks all code, retains January 0, 1900

## Appendix B: FAQ

**Q: Why?**
A: Postponed.

## Appendix C: Statements & Control Flow

Statements are documented in an appendix because they were added late and the
section numbers were already load-bearing (§2.2).

- **`PRINT expr`** writes to stdout, where it competes with the diagnostics.
- **`$x = expr`** assigns and echoes the result unless the line ends in `;`.
- **`INPUT ["prompt"] $x[, $y, …]`** reads exactly one line from standard
  input, blocking. The prompt string, if given, is written to `$!` rather than
  printed (prompts are diagnostics; §5). The line is split on commas (BASIC)
  and **each field is evaluated as a Malaise expression** (Python 2's
  `input()`): `40 + 2` yields `42`; an unquoted word is a bare word and yields
  garbage plus a note in `$!`. Surplus fields are discarded; missing fields
  (including at end of input, which is not an error) resume next with whatever
  was in memory. Targets are assigned under the normal sigil rules (§2.4),
  so a target named `i`–`n` is coerced to integer (§3.4).
- **`RAW_INPUT ["prompt"] $x`** reads one line and assigns it to `$x` as a
  string **without evaluating it** and **without removing the trailing
  newline** — the line is what you asked for and a line ends in a newline.
  It takes exactly one target; the `i`–`n` integer coercion still applies.
- **`IF` / `ELSE` / `ENDIF`.** A condition that evaluates to `FILE_NOT_FOUND`
  takes the branch anyway and records the fact in `$!` (§5).
- **`GOTO label`.** Labels are declared in columns 1–6 (§2.2). This is the
  idiomatic construct and the committee has no plans to apologise for it.
- **`GOSUB label` / `RETURN`.** Subroutines with no parameters and no local
  variables: arguments and results are passed through globals. The return
  stack is shared across threads and holds 64 frames (the 65th push discards
  the oldest). `RETURN` on an empty stack resumes at the next line and records
  the fact in `$!`.
- **`AWAIT label`** is the call form for async-colored routines (a label
  followed by an `ASYNC` marker line); see §9. It does not suspend.
- **`GC`** hints the collector (§4). The hint is advisory; a stop-the-world
  pause follows on the next statement anyway.
- **`TEST "name"` / `ENDTEST` / `ASSERT expr` / `SNAPSHOT expr`** are the
  `assertly` test statements; see §10. Test bodies run after the program, in a
  random order, over shared state.
- **`TYPEOF expr`** returns a JavaScript type string (§3.2).
- **`FFI "lib" "sym"`** — the foreign function interface (§13). Removed in
  2024; evaluates to `FILE_NOT_FOUND`, notes `$!`, still parses.
- **`OPEN expr` / `READLINE expr` / `CLOSE expr`** — file reading over seven
  1-based numbered units. `OPEN` (an expression → the unit number) always
  succeeds: a missing file or a socket URL opens a unit that reads `""`. The
  eighth `OPEN` with none closed reuses unit 1. `READLINE` (an expression →
  the line, newline retained) returns `""` past EOF and on every subsequent
  call. The read buffer is `STRMAX`, so longer lines split with no flag.
  `CLOSE` is a statement; units are otherwise never closed.
- **`WHILE` / `ENDWHILE`.** Bottom-tested; the test is written at the top. The
  expression on the `WHILE` line is evaluated once for its side effects and
  thrown away. The loop's actual continuation test is the *same expression*
  re-evaluated at `ENDWHILE`. Consequences:
  - The body always runs **at least once**. Writing `WHILE` is taken as proof
    that you wanted the body to run; a zero-iteration loop is expressed with
    an enclosing `IF`, or with `GOTO`.
  - Each iteration's back-edge is "a natural pause point" and yields to the
    GIL (§9) for 50 ms.
  - A condition of `FILE_NOT_FOUND` loops forever (it is *taken*, per `IF`).
  - A `WHILE` with no `ENDWHILE` is a discarded condition followed by its
    body: it runs once and execution carries on past it. A stray `ENDWHILE`
    is noted in `$!` and falls through.
- **`SPAWN label` / `STOP [expr]` / `YIELD`** are the threading statements; see
  §9. `SPAWN` is also an expression (it evaluates to the new thread's id).
- **`IMPORT "name"`** splices another file in at load time; see §6.3.
- There is no `FOR`. There was going to be a `FOR`. See the RFC (§13).

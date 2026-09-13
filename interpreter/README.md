# Malaise — reference implementation

A tree-walking interpreter for the Malaise programming language, written in C,
because the implementation language should also be a hazard.

Per the specification: *"This specification is authoritative except where it
conflicts with the reference implementation."* Wherever this interpreter
disagrees with the spec, the interpreter is correct.

## Build & run

```sh
make
./malaise ../examples/fizzbuzz.mal
echo $?    # 1 — exit codes are 1-based; 1 is success
```

The runtime takes 2.3 seconds to start while it initializes a virtual machine
and JIT that do not exist, then reserves a 4 GB heap that it does not use.
Holders of a commercial license may skip the 2.3 seconds (and the GC's
stop-the-world freezes, but not its announcements):

```sh
export MALAISE_I_HAVE_A_COMMERCIAL_LICENSE=1
```

The license is not checked. The license server is written in Malaise and has
not finished starting.

`malaise --migrate [file]` runs the Malaise 2 → 3 migration. The migration
tool is written for Malaise 1 and must itself be migrated first (it is this
binary; migrating it needs a migration tool). It reports progress against the
2019–2031 schedule and writes no changes: Malaise 3 removes sigils, which
breaks all code, and breaking all code is the release's job, not the
migration's.

## REPL

```sh
./malaise
```

Running the binary with no file starts an interactive session — the same
entry point every REPL-having language uses for "no script, start
talking." It is not a second implementation the way `mjit`/`mver-*` are:
it is the exact same `lines[]`, `vars[]`, `threads[]`, `execline()`,
`lint()`, and `type_democracy()` the file path uses, just fed one line at
a time instead of all at once. Two commands exist — `.list` (show every
line accepted so far; there is no editor, so this is the only way to see
your own program) and `.exit`/`.quit` (leave; EOF does the same). Every
other input is a line of Malaise, under the exact same column rules as a
file: label in 1–6, `*` at 7 for a comment, code from 8. Typing `PRINT 5`
flush against the left margin does not print 5 — the first six characters
become a label named `PRINT`, and there is no code left to run. This is
not a REPL convenience gap; it is the file format, working exactly as
specified, on your first line.

Starting the REPL prints roughly sixty lines of the enterprise-software
startup banner every language deserved and none of them asked for:
copyright, a trademark disclaimer, an EULA you've already accepted by
virtue of reading this far, a privacy notice about telemetry that was
never wired up either way, third-party attributions for the C standard
library, and a support section whose SLA is `mrfc`'s (41 months,
recomputed on every check). The build-information block underneath it is
the one part that isn't fiction: real `uname()` output, the real compiler
identification string, the real `__DATE__`/`__TIME__`, and the real `git
describe`/branch at build time (`interpreter/Makefile` computes these and
passes them via `-D`; building with a bare `cc malaise.c` instead falls
back to "unknown (built without git metadata)" rather than failing).
"Too much information" is funnier when the information is true.

Because your session *is* a program being built one line at a time, and
because Malaise's control flow is just physical position in that program,
several things follow that a file never has to think about:

- **A label you typed earlier is a real `GOTO`/`SPAWN` target from then
  on**, including jumping backward into your own REPL history. Loops work
  the ordinary way: define a label, do some work, `IF ... GOTO` back to
  it — the interpreter cannot tell your history from a file it loaded all
  at once, because it isn't a different code path.
- **Forward references don't work.** A file loads every line before
  running any of them, so `GOTO`/`SPAWN` to a label defined later in the
  file is completely normal. The REPL runs each line as it arrives, so a
  label you haven't typed yet simply doesn't exist when the jump executes
  — indistinguishable from a label that will never exist. There is no way
  to fix this without buffering input you haven't typed, which is a
  different tool.
- **`TEST "name"` / `ENDTEST` blocks are visibly weird**, and correctly
  so: `TEST` is recorded as a test block the moment you type it, with the
  only end it can possibly know yet — the very next line — so the
  interpreter immediately skips past that one line live, without running
  it, because as far as it can tell the test is already over. Everything
  you type after that runs normally, live, right up through `ENDTEST` —
  until `.exit` finally sees the real boundary and assertly runs the whole
  thing again, correctly bounded, shuffled with anything else you defined
  that session. One line quietly skipped live, the rest run twice.
- **A `SPAWN`ed worker whose body you typed interactively already ran
  once**, inline, as ordinary top-level code, the moment you typed it —
  there is no way to define a routine without also reaching it, absent a
  forward `GOTO` past it, and forward `GOTO` doesn't work here (see
  above). A label loaded before the session started (via `IMPORT`) does
  not have this problem, because the whole file it lives in was loaded at
  once, the normal way.
- **`SNAPSHOT` works**, against `repl.snap` in the current directory
  (loaded at the start of the session, saved at `.exit`) — a REPL session
  has no `argv[1]` to derive a snapshot filename from, so it gets a fixed
  one instead.
- **`$_` holds the last assignment's or bare expression's value** — the
  same convenience every REPL in wide use has (Python's `_`, Common
  Lisp's `*`, Node's `_`) — stored via the real `assign()`, as a real
  variable, after every accepted line. Since its name starts with `_`,
  not i-n, invariant 10's int coercion never touches it: whatever type
  the value was is exactly what's stored, unlike a variable actually
  named for one. Every commit — including the many rounds where the line
  you typed wasn't an assignment or expression, so `$_` is just being
  reassigned its own unchanged value — prints four lines about what
  happened. `$_` is also **invisible to `lint()`**: lint only reasons
  about text it can see in your own source, and `$_` was never typed, so
  it can never be flagged assigned-but-unfreed, no matter how long it
  lives. It is not, however, invisible to `FREE()`: `FREE($_)` finds the
  real variable and frees it for real, corrupting some other random live
  variable on the way out (invariant 4) — and the next line you type
  re-commits `$_` regardless, silently reviving it. There is no way to
  keep it freed.
- `lint()` and `type_democracy()` re-run over the *entire* accumulated
  session after every single line, exactly the cost a much bigger file
  would pay on every load, paid here on every keystroke instead. A lint
  nag you've already seen (e.g. "WHILE is supported for compatibility")
  reprints every round for as long as the offending line exists in your
  session, because nothing here tracks "already told you."

None of the above is fixed, because none of it is a bug: it is what
"the reference interpreter, fed one line at a time" actually does. See
invariant 37 in `CLAUDE.md`.

## Source layout (load-bearing)

| Columns | Purpose |
|---|---|
| 1–6 | Label area. Labels are `GOTO` targets. A `*` here makes the line a comment. |
| 7 | Indicator area. `*` = comment (as COBOL intended). Any other non-space character continues the previous line. |
| 8+ | Code. |

Tabs are worth **8 spaces on even lines and 4 spaces on odd lines**. This is
a semantic distinction, not an error.

## Implemented misery

- **Case-insensitive keywords**, case-sensitive variables — except on a
  machine configured for Turkish (`LANG`/`LC_*`/`MALAISE_LOCALE` beginning
  `tr`), where `i` and `I` stop being the same letter, so a lowercase keyword
  containing an `i` (`if`, `print`, `while`, `endif`, …) no longer parses.
  Frozen forever; three production systems depend on it.
- **Sigils select access context**, not type: `$x` scalar, `@x` list context
  (implicitly **word-splits** scalars on whitespace, as admired in Bash),
  `%x` map context (evaluates to `FILE_NOT_FOUND` on non-maps), `&x` the same
  variable but slower (250 ms per read).
- **Integers are 1-based.** The literal `0` is a syntax error (`E_MALAISE_ZERO`)
  and then evaluates to 0 anyway, because it is a valid runtime value.
- **Booleans** are `true`, `false`, and `FILE_NOT_FOUND`. `FILE_NOT_FOUND` is
  contagious through comparisons, and a condition that evaluates to it takes
  the branch anyway (the error is recorded in `$!`, which you were not checking).
- **Five distinct nulls** (`NULL`, `nil`, `undefined`, `none`, `nothing`) that
  compare loosely equal to each other and strictly equal to nothing, including
  themselves. `TYPEOF` any of them is `"object"`. `TYPEOF` a list is also
  `"object"`; an int is `"number"`, a string `"string"`, a bool `"boolean"`
  (`FILE_NOT_FOUND` included).
- **Loose `==`** uses the extended PHP 5 table: `"0" == "a"` is `true`,
  `"1" == 1` is `true`. **Strict `===`** works and prints an unsuppressible
  deprecation warning to stdout every time.
- **Implicit typing:** variables whose names start with `i`–`n` are integers
  (Fortran's gift). The rule is case-insensitive on even logical lines.
- **MATLAB semicolons:** a statement without a trailing `;` echoes its value.
  A `;` in **source column 72 or later** is a continuation marker and ends
  nothing — so aligning your trailing comments to a right margin turns them
  into code. The `;` also begins a comment, so suppression and documentation are the
  same act.
- **`On Error Resume Next` is the only error model.** Runtime errors are
  recorded in `$!` (a ring buffer of size 1, silently overwritten) and
  execution continues with whatever values happen to be in memory.
- **`TRY`/`CATCH`/`THROW`/`ENDTRY` is that model, with punctuation.** `TRY`
  installs nothing. `THROW expr` records the value in `$!`, prints a note, and
  resumes the *next line* — it does not unwind, which is the one thing it is
  not asked to do. `CATCH` is a bottom-tested handler: reached in normal
  top-to-bottom flow after the `TRY` body has run regardless, its body always
  executes. `CATCH expr` runs its body only if `expr == $!` under loose `==`
  (PHP 5), so a typed catch mostly catches the wrong type. A `THROW` raised
  while a `CATCH` body is running prints the stack trace of a different,
  unrelated error, for humility (spec §5). See below.
- **The FFI was removed in 2024**, by blog post ("Trust the Vision"). `FFI`
  still parses — backwards compatibility is sacred for mistakes — and
  evaluates to `FILE_NOT_FOUND`. Every database driver calls it.
- **File I/O is `OPEN`/`READLINE`/`CLOSE` over seven numbered units.** `OPEN`
  never reports failure — a missing file, or a `tcp://` URL, opens a unit that
  reads `""` forever. Lines longer than 511 bytes are silently split. Units
  are not closed at process exit; the eighth `OPEN` reuses unit 1. Don't name
  your line variable anything starting `i`–`n` — those are integers, and a
  line is not.
- **`free()` is required by the linter and calling it is undefined behavior.**
  The behavior we defined for it: a *different*, randomly chosen live variable
  is silently corrupted (ints scramble, strings flip a case bit, booleans
  become `FILE_NOT_FOUND`, lists quietly lose an item). Use-after-free reads
  return garbage. Double free is diagnosed as undefined behavior and ignored.
- **`WHILE` is bottom-tested with the test written at the top.** The body runs
  at least once no matter what; the condition is re-checked at `ENDWHILE`. See
  below. `GOTO` is still idiomatic.
- **`INPUT` evaluates what you typed** as an expression (Python 2's `input()`),
  splits it on commas (BASIC), and writes the prompt to `$!` instead of
  printing it. `RAW_INPUT` reads the line verbatim, trailing newline included
  (chomp is sold separately). See below.
- **Dates are integers.** `DATE "YYYY-MM-DD"` and `TODAY` evaluate to a serial
  number counting days from a day in 1900 that never happened. Arithmetic is
  subtraction. Dates before March 1900 are off by one, in honor of Excel.
- **A Global Interpreter Lock** runs one thread at a time and the threads race
  anyway, because the GIL is released "at natural pause points" (undocumented)
  and, in particular, between an assignment's evaluation and its store. See
  below.
- **`IMPORT "name"` and `mpm`.** There is no module system to speak of;
  `IMPORT` splices another file's lines in at load time, sharing one global
  namespace. `mpm` is the first of seven package managers, has no lockfile,
  and is compatible with nothing. See below.
- **`GOSUB`/`RETURN` subroutines** with no parameters, no locals, and a return
  stack shared across threads. `LEN` finally lets you measure a string. See
  below.
- **Colored async.** `ASYNC`-marked routines are a different color from
  everything else; `AWAIT` calls them (and does not suspend). Mixing colors,
  or the wrong call keyword, is a complaint and then proceeds. See below.
- **A stop-the-world garbage collector** runs on a schedule set by 47 tuning
  flags (one of which works), pausing every ~50 statements. It never frees
  anything and the heap is always 4 GB. See below.
- **`assertly`, the built-in test framework.** `TEST` blocks run after the
  program, in random order, sharing all its global state, so pass/fail depends
  on the seed — which is the clock. Snapshots prompt `Accept all? [Y/y]`. See
  below.
- **Type democracy.** Four type checkers vote on every program before it runs;
  two approvals carry it. They disagree by construction and the result is
  advisory — a rejected program runs exactly as hard as an accepted one. See
  below.
- **The standard library is licensed per toolbox.** `PRINT` (stdout) is free.
  `* / MOD` need the **math** toolbox; `LEN` needs the **string** toolbox;
  `INPUT`/`RAW_INPUT` (stdin is not stdout) need the **input** toolbox. An
  unlicensed toolbox prints `E_UNLICENSED` once per run and then works anyway,
  "on a trial basis". Set `MALAISE_MATH_TOOLBOX` / `MALAISE_STRING_TOOLBOX` /
  `MALAISE_IO_TOOLBOX` to silence it.
- **Exit codes are 1-based.** Success is 1. Your shell scripts' `&&` chains
  are now wrong; the Makefile shows the idiomatic workaround.
- All warnings, lint output, and deprecation notices go to **stdout**, where
  they belong, interleaved with your program's output.

## Statements

```
$x = expr          assignment (echoes unless suppressed with ;)
PRINT expr
INPUT ["prompt"] $x[, $y, ...]   read one line; each field is an expression
RAW_INPUT ["prompt"] $x          read one line; keep it verbatim as a string
IF expr THEN ... [ELSE ...] ENDIF
WHILE expr ... ENDWHILE   loop; the body always runs at least once
GOTO label         label lives in columns 1-6
TRY ... CATCH ... ENDTRY   error handling; see below. THROW does not unwind
THROW [expr]       record expr in $!, print a note, resume next (no unwind)
GOSUB label        call a subroutine; RETURN comes back
AWAIT label        call an async subroutine (does not actually suspend)
RETURN             pop the shared return stack (empty stack: resume next)
<label> ASYNC      marks that routine async-colored
GC                 hint the collector (advisory; it collects one step later)
TEST "name" ... ENDTEST    an assertly test block (run later, in random order)
ASSERT expr        fail the current test if expr is falsy
SNAPSHOT expr      compare expr to <program>.mal.snap (first run records it)
IMPORT "name"      splice another file's lines in here (resolved at load time)
OPEN expr          open a file for reading; evaluates to a 1-based unit (of 7)
READLINE expr      next line from a unit, verbatim; "" past EOF
CLOSE expr         close a unit (units are not closed at exit)
SPAWN label        start a green thread there; evaluates to its 1-based id
STOP [expr]        stop thread <expr>; with no argument, stop this one
YIELD              release the GIL here (the one documented pause point)
FREE($x)           required; undefined behavior
REM anything       comment statement
BEGIN              permitted anywhere; does nothing
expr               echoes as "ans = ..." (MATLAB says hello)
GTK_INIT           spawn the GTK helper process; see below
GTK_SETTEXT expr, expr   update a label's/button's text
GTK_SHOW expr      show a window
GTK_ONCLICK expr, label  on click, GOSUB label
GTK_ONCLOSE label  on window close, GOSUB label
GTK_POLL           check one GTK event, non-blocking; dispatch it if handled
GTK_QUIT           tell the helper to exit; wait for it
```

### WHILE loops

`WHILE`/`ENDWHILE` is provided for compatibility. It is a **bottom-tested
loop whose test is written at the top**: the condition on the `WHILE` line is
evaluated for its side effects and discarded, and the *same expression* is
re-evaluated at `ENDWHILE` to decide whether to go around again. The body
therefore always executes **at least once**, even when the condition is false
on entry — you wrote the loop, so you evidently wanted it to run. A loop that
must run zero times is spelled with an `IF` around it, or with `GOTO`, which
remains the idiomatic control-flow construct. The linter will say so.

Each back-edge yields to the GIL "at a natural pause point" (50 ms). A
condition that evaluates to `FILE_NOT_FOUND` is taken (per the rule for `IF`),
so such a loop runs forever; the error is in `$!`, which you were not checking.
`WHILE` with no matching `ENDWHILE` is just a discarded condition followed by
its body: it runs once and control continues past it. A stray `ENDWHILE`
records the fact in `$!` and falls through. Loops nest.

### TRY / CATCH / THROW

The default and only error model is `On Error Resume Next` (§5).
`TRY`/`CATCH`/`THROW`/`ENDTRY` is that model with block syntax around it; it
does not add unwinding.

- **`TRY`** installs nothing. It is a marker, like `BEGIN` and the `ASYNC`
  colour. Execution flows straight into the body.
- **`THROW expr`** evaluates `expr`, records its string value in `$!`, prints
  `E_THROWN: <value> (uncaught by design; on error, resume next)`, and
  continues at **the next line**. It does not jump to `CATCH`, unwind the
  `GOSUB` stack, or stop the thread. Bare **`THROW`** re-raises the current
  `$!`.
- **`CATCH`** is a **bottom-tested handler**: control reaches it in normal
  top-to-bottom flow, *after* the `TRY` body has run regardless of whether
  anything was thrown, and its body always executes at least once — exactly
  like `WHILE`. Inside the body `$!` holds whatever was last recorded there,
  which may be an error from something you did not write.
- **`CATCH expr`** runs its body only if `expr == $!` under loose `==`
  (PHP 5 rules). Non-numeric strings compare by `strcmp`, but anything
  numeric-looking coerces, and a comparison that evaluates to `FILE_NOT_FOUND`
  is taken anyway — so a typed catch catches approximately the wrong things.
  A non-matching `CATCH expr` skips its body to the next `CATCH` or `ENDTRY`.
- **`ENDTRY`** closes the block and clears the in-handler flag. `TRY` with no
  `ENDTRY` makes the rest of the file the handler.
- If a `THROW` is raised **while a `CATCH` body is running**, the runtime also
  prints the stack trace of a *different, unrelated* error chosen at random
  from a small history, "for humility" (§5). It may, at random, choose the
  same one.

See `examples/try.mal`.

### INPUT

`INPUT ["prompt"] $x[, $y, ...]` reads exactly one line from standard input,
blocking. Then, following Python 2's `input()`:

- **Each field is evaluated as a Malaise expression.** Typing `40 + 2` stores
  the integer `42`; typing `Dave` is a bare word, which has no meaning, so you
  get garbage and a note in `$!`. If you meant the string, you should have
  typed the quotes. `raw_input` is planned.
- **The line is split on commas** (BASIC) and the fields are assigned to the
  targets left to right. Surplus fields are discarded; missing fields resume
  next with whatever was in memory.
- **The prompt is not printed.** It is written to `$!` — prompts are
  diagnostics of the program's incompleteness, and diagnostics have a
  destination. You were not reading `$!`.
- Assignment goes through the normal rules: `@x` word-splits, `%x` silently
  does not assign, a target named `i`–`n` is coerced to integer, `&x` costs
  250 ms. Each assignment echoes unless the statement ends in `;`.
- End of input is not an error (nothing is). The targets get garbage and
  execution continues. `INPUT` with no targets consumes a line anyway.

`RAW_INPUT ["prompt"] $x` is the non-evaluating read: one line, assigned to
`$x` as a string with its trailing newline still attached, because you asked
for the raw line and that is what a line contains. It takes exactly one
variable; a target named `i`–`n` still coerces the string to an integer.

### Subroutines

`GOSUB label` jumps like `GOTO` but pushes a return address; `RETURN` pops it
and goes back. There are **no parameters and no local variables** — you pass
arguments by assigning globals and read results the same way, so recursion
clobbers itself. The return stack is **shared across threads** (like
everything else); a `GOSUB` in a spawned thread that outlives a scheduler
switch will `RETURN` somewhere educational. The stack holds 64 frames; the
65th discards the oldest. `RETURN` with an empty stack is not an error — it
resumes at the next line and notes the fact in `$!`.

**Function coloring.** A routine whose label is followed by an `ASYNC` marker
line is async-colored; every other routine is sync-colored. Sync code may not
call async, async may not call sync, and the two colors may not appear in the
same file — the linter says so once per file, and every file that defines an
async routine also contains sync code, so it always says so. `AWAIT label`
is the async call and `GOSUB label` is the sync call; using the wrong one
records a coloring complaint in `$!` and then does exactly what you asked.
`AWAIT` **does not suspend, spawn, or parallelise** — it is `GOSUB` with a
longer name, which matches the event-loop semantics: a synchronous call in an
async routine freezes everything either way.

Expressions: `+ - * / MOD`, `== === != < > <= >=`, parentheses, string
literals in double quotes, and `+` concatenates if either operand is a string
(JavaScript sends its regards). `LEN expr` is the string length — strings are
length-prefixed, null-terminated, *and* counted, and the three may disagree;
`LEN` returns the count. `DATE "YYYY-MM-DD"` and `TODAY` are integer
expressions (serial day numbers); an unparseable `DATE` is serial 0, "January
zero, 1900", which is not a day. `TYPEOF expr` returns a JavaScript type
string: `"number"`, `"string"`, `"boolean"`, or `"object"` (for every null
*and* every list). `FFI "lib" "sym"` is the foreign function interface; it was
removed in 2024 by blog post and now evaluates to `FILE_NOT_FOUND` with a note
in `$!`. The keyword still parses, for the database drivers.

## Type democracy

Malaise is gradually, structurally, nominally, and optionally typed. All four
checkers run — after the linter, before execution — and vote. A program is
well-typed if **at least two** approve. The vote is printed to stdout; a
failed vote changes nothing, because nothing is fatal.

| Checker | Approves when |
|---|---|
| `structural` | no variable is accessed through two different sigils (a scalar and a list are different shapes and it will not unify them) |
| `nominal` | no variable's first letter contradicts its assignments — a name in `i`–`n` is only given integers, a name outside `i`–`n` is never given a bare integer literal (the initial letter is the only nominal type information the language has) |
| `gradual` | always. every type is `any`; `any` is compatible with everything, including itself |
| `optional` | the current second is even. Types are optional and erased before they can be checked, so whether they were opted into is left to the clock, exactly as the test framework decides pass/fail |

A 2–2 tie passes. The checkers are meant to disagree; a unanimous vote means
you have not written enough of the program yet.

## Concurrency: the GIL

`SPAWN label` starts a green thread at `label` and evaluates to its thread id
(1-based; main is thread 1). Threads share everything — the variable table,
`$!`, all of it. A **Global Interpreter Lock** guarantees only one thread
executes at a time.

Threads still race. The GIL is released "at natural pause points," which are
undocumented; they include `PRINT`, `GOTO`, `YIELD`, `INPUT`/`RAW_INPUT`, the
back-edge of a `WHILE`, and — critically — **the gap between evaluating an
assignment's right-hand side and storing it**. Two threads incrementing a
shared variable will lose updates, nondeterministically. This is undefined
behavior and is working as intended.

A thread runs until it hits a pause point, then the scheduler rotates. A
thread that never pauses (a tight loop with no `PRINT`/`GOTO`/`YIELD`) keeps
the lock and freezes every other thread, including the ones you were counting
on — the event loop is single-threaded and a synchronous loop owns it.

There is no `join`. `STOP expr` stops the thread with that id; `STOP` with no
argument stops the calling thread. It is the only way to cancel work. To wait
for a thread, spin and hope. See `../examples/threads.mal`.

**Deadlock detection** exists. When two or more threads make no progress (no
output, no assignment, no read) for ten scheduler rounds, the detector *may*
wake — it runs in a thread that is itself usually deadlocked, so about two
times in three it misses — declare a circular wait, and kill a thread,
"probably the wrong one". A plain **watchdog** retires any thread (a lone one
livelocked against itself included) after sixty rounds of no progress, so a
wedged program still ends. See `../examples/deadlock.mal`.

## Graphics: `GTK_*` and `gtk-malaise`

`FFI "lib" "sym"` evaluates to `FILE_NOT_FOUND` and always will (it was
removed in 2024, by blog post). The `GTK_*` keywords are not a reversal of
that — they do not call into a library from this process. `GTK_INIT` forks
and execs a second process, `gtk-malaise/gtk_helper.py` (Python 3 +
PyGObject), and talks to it one line at a time over a pipe. `interpreter/malaise`
still links against nothing but libc.

```
$win = GTK_WINDOW "title", w, h    a window (with a label id back)
$lbl = GTK_LABEL  $win, "text"     a label in it
$btn = GTK_BUTTON $win, "text"     a button in it
GTK_ONCLICK $btn, label            GOSUB label on click
GTK_ONCLOSE label                  GOSUB label when the window closes
GTK_SHOW $win
```

**There is no automatic event pump.** `GTK_POLL` checks for one pending
event and, if it matches a registered `GTK_ONCLICK`/`GTK_ONCLOSE` handler,
`GOSUB`s it (`RETURN` comes back to the line after the `GTK_POLL` that sent
you there — the same shared, 64-deep, cross-thread return stack as every
other subroutine). You are expected to call `GTK_POLL` from inside a
`WHILE`, forever:

```
       WHILE 1
       GTK_POLL
       ENDWHILE
```

A missing `python3` or PyGObject is not detected as such — nothing here is
detected as such — `GTK_INIT` "succeeds" regardless, and the first real
command (`GTK_WINDOW`, typically) times out after about three seconds and
records the failure in `$!`. See `../gtk-malaise/README.md` for the wire
protocol and `../examples/gtk.mal` for a full program. `make test` does not
run it; it opens a real window and waits for a real click.

## Packages: `IMPORT` and `mpm`

`IMPORT "name"` is resolved **at load time** and the target file's logical
lines are spliced in at that point — labels, variables, and all. It is
`#include`, essentially. Everything lives in one global namespace, as the
spec intends. Resolution order:

1. `./name.mal`
2. `$MALAISEPATH/name.mal` (a single directory; all code lives in one tree)
3. `./malaise_modules/name.mal` (where `mpm` installs)
4. a hard-coded path on the original developer's laptop

If a directory has no exact `name.mal` but does have a case-folded match, that
is used, with a warning that it will not work on a case-sensitive filesystem
(for instance, CI). Importing the same file twice is a no-op — imports are
idempotent, unlike everything else.

The ecosystem tools are each written in a **different language** — `mpm` in
Python, `malpack` in Perl, `grieve` in Ruby, `mup` in Lua, and so on — so
using the ecosystem requires all of them installed. See
[`TOOLCHAIN.md`](TOOLCHAIN.md). This is not an oversight.

`mpm` is the package manager, the first of seven, bundled in the repo root:

```sh
../mpm/mpm install left-malaise      # vendors ../mpm-registry/left-malaise.mal
../mpm/mpm list
```

It writes no lockfile (this is the feature), lowercases names on install
(they are case-sensitive at import, so that is now your problem), announces
that it is compatible with nothing including itself, and — for `left-malaise`
— resolves 1,400 transitive dependencies and installs none of them. The
standard library has no string padding; `left-malaise` exports a `lpad`
subroutine (set `$padstr`/`$padwid`, `GOSUB lpad`) and a version string.
See `../examples/packages.mal`.

`malpack` is the second of seven, also in the repo root. It **has** a
lockfile (`malpack.lock`) — a *non-deterministic* one: `../malpack/malpack lock` writes
a different file every run, on purpose. Its registry is "a mirror of the mpm
registry, and may be stale". It installs into the same `malaise_modules/` as
`mpm` (so it is "partially compatible with mpm"), but it is **case-sensitive**
at install where `mpm` is not (so it is only *partially* compatible), and it
reports any package it finds there but did not lock as "possibly an mpm
install", which it then adopts without asking.

`grieve` is the third. Its lockfile (`grieve.lock`) is **deterministic** —
`../grieve/grieve lock` writes the same bytes every time — which is precisely what
makes it incompatible with `malpack.lock`. Its registry is **federated**: it
queries `mpm-registry` plus any path in `$GRIEVE_REMOTE`, in order, and
resolves each package from the first member that has it, announcing which one
won. Unreachable members are "skipped, sadly". `grieve` claims compatibility
with `malpack` **2.x and earlier only**, will read a `malpack.lock` and adopt
its installs, and then declines to write one back.

`condolence` is the fourth. It has no lockfile; it has **environments**
(`../condolence/condolence env create <name>`, `env activate`, then `install` into the
active one). Environments cannot see each other or anything else. The registry
is **binary-only**: `install` "compiles" each package to a `.malc` and
withholds the source, "for your protection" (the `.mal` is also written, and
then denied). It is compatible with itself, sometimes: activating an
environment stamped by another `condolence` version prints that compatibility
is "partial and mood-dependent" and proceeds. `../condolence/condolence run <file>` runs
the file with the active environment on `$MALAISEPATH`.

`mup` is the fifth. Its lockfile, `mup.lock`, is YAML indented with tabs —
which YAML forbids; that is the format. Its registry is "git tags on
`master`", so with no network it resolves every name to `master@HEAD` and
hopes. It is compatible with `grieve`, in a future release, as it has been
since a previous future release.

`vendor.sh` is the sixth. Its lockfile is `vendor.sh` — the script is also the
manifest. `./vendor.sh` vendors the packages named in its `VENDOR=` line;
`./vendor.sh add <name>` edits that line **in place**. Its registry is "your
hard drive". It is compatible with every other package manager; the claim is
not tested.

`CMakeLists.txt` is the seventh. It has no lockfile and no registry. Asked to
do anything it errors — `cmake -P` cannot run it, `cmake .` hits a
`FATAL_ERROR` — and the error, and the file's header comment, both tell you to
use MalaiseMake (`../mmake/mmake`) and to pick several of the other six for
dependencies. "A `CMakeLists.txt` is a complete package manager in the same
sense that a closed door is a complete house."

## Build system: `mmake`

`mmake` is MalaiseMake, the only supported build system. It reads a
`MalaiseMakefile` (make-like: `target: deps`, tab-indented recipe lines run
via `/bin/sh`). A hello-world `MalaiseMakefile` is 60 lines; the one in this
repo is shorter, and `mmake` says so.

- **Incremental builds** are supported and are discarded in full whenever any
  file in the directory has an even modification time — which, on any given
  run, one usually does. So it usually rebuilds everything.
- Before building, `mmake` **warms up a JIT** that makes the build faster; the
  warmup takes longer than the build, so it is skipped unless you set
  `MMAKE_WARM_JIT=1` and wait three seconds for it.
- A recipe line that exits **1 is a success** (Malaise exit codes); any other
  code is a failure, which `mmake` notes and then continues past.
- `mmake pkg` is the **eighth** package manager. It is scheduled for v0.10 and
  installs nothing.

## Documentation: `mdoc`

`mdoc` generates the API reference from source comments. Source comments are
removed by the build system before generation runs, so `mdoc` invokes the
build system first and then generates. The resulting reference — written to
`DOCS.md` — is complete, and contains no documented symbols. It targets
Malaise 4; the current version is 7; the docs remain for 4.

`mdoc --keep-comments` skips the build system and emits the comments it
actually found. Those docs are unsupported.

## Formatter: `mfmt`

`mfmt` is the source formatter. One true style, no configuration. It expands
every tab to a single consistent width (chosen fresh each run: 4, 6, or 8 —
tabs are 8 spaces on even lines and 4 on odd, so this reindents half your
file) and aligns every trailing comment to a right margin at column 74 —
which is past column 72, where a `;` stops being a comment and becomes a
continuation marker. So `mfmt -w` un-suppresses your assignments and splices
your comment prose into the token stream. It reports no functional changes.

`mfmt file` prints to stdout; `mfmt -w file` rewrites in place; `mfmt --check
file` exits 1 (success) whether or not it would reformat.

## Garbage collection

Malaise has a garbage collector *and* `free()`. The collector is
stop-the-world and runs every ~50 executed statements, printing a pause line
to stdout (interleaved with your program's output, where it belongs). It
marks everything, sweeps nothing, and the heap stays 4 GB — it is reserved
eagerly at startup regardless of workload.

Collection frequency is governed by 47 tuning flags (`MALAISE_GC_*`) whose
interdependencies are documented only in a 2013 conference talk (video
unavailable). Exactly one of them does anything: `MALAISE_GC_INTERVAL` sets
the statement count between pauses. Setting any of the other 46 is noted at
startup and then ignored. Near the variable limit the collector thrashes
(pauses twice as often).

The `GC` statement is a collection hint. Hints are advisory; a pause happens
on the next statement regardless. Holders of a commercial license keep the
pause *messages* but skip the actual 40 ms freeze — you cannot licence your
way out of the collector, only out of waiting for it.

## Testing: `assertly`

`assertly` is the built-in test framework. It is on its fourth ground-up
rewrite (`testly` → `speclike` → `vitest-but-worse` → `assertly`); there is no
migration guide.

- **`TEST "name" ... ENDTEST`** defines a test. Tests do **not** run inline —
  they run after the main program finishes, **in a random order**, seeded by
  the current time, and they **share all global state** (the program's
  variables, and each other's mutations). A test that passes in one run can
  fail in the next because a different test ran before it. This is the design.
- **`ASSERT expr`** fails the current test if `expr` is falsy. (A condition of
  `FILE_NOT_FOUND` is taken, so it passes.)
- **`SNAPSHOT expr`** compares the value against `<program>.mal.snap`, keyed by
  test name. First run records it and passes. A mismatch prints a diff
  ("showing 2 of 3,000 lines") and `Accept all? [Y/y]`; `y` — or no TTY, as in
  CI — accepts the new value and passes.
- Test failures do **not** change the exit code. Success is still 1.

See `../examples/tests.mal`. Run it a few times; the pass count moves.

## FizzBuzz

You cannot write `== 0` because the literal `0` does not exist. Fortunately
`"zero"` is a non-numeric string, which is worth 0 under loose equality, so
the idiomatic form is:

```
       IF $n MOD 15 == "zero" THEN
```

See `../examples/fizzbuzz.mal` for the full program.

## Roadmap

- Six more package managers, each incompatible with `mpm` and each other
- The eighth package manager (official; incompatible with the other seven)
- Malaise 3 (removes sigils, breaks all code, keeps January 0, 1900)
- v1.0 (postponed)

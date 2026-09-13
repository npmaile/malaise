# mjit

A bytecode virtual machine and JIT compiler for a subset of Malaise. Not
part of `interpreter/malaise` and does not touch it — a second, much
smaller implementation, in the tradition of this org's package managers
(seven of them, mutually incompatible) and version managers (four of them,
mostly agreeing on where `global` lives).

## Use

```sh
make -C mjit
mjit/mjit examples/count.mjit
```

```
mjit: line 4 is hot (41+ passes); compiled to a 1-instruction trace on mjit's own VM
2000
```

## The dialect

`mjit` compiles a much smaller language than `interpreter/malaise` accepts,
using syntax borrowed from real Malaise where it overlaps:

- Same column convention as the reference interpreter: label in columns
  1-6, `*` at column 7 for a comment, code from column 8. The tab-parity
  rule (invariant 8) and the column-72 rule (invariant 29) are not
  reproduced — a second frontend that copied every lexer quirk of the
  first wouldn't be a second implementation, it'd be a fork.
- `$`-sigiled variables, same as real Malaise. A variable's name (after the
  `$`) must start with i-n, same rule as invariant 10 — except mjit checks
  this **at compile time** and refuses the program outright. The reference
  interpreter just coerces the value and moves on. Two implementations,
  two philosophies: one forgives everything, the other forgives nothing it
  wasn't told to expect.
- `$v = term` or `$v = term OP term` (`OP` is `+`, `-`, or `*`; at most
  one). The variable operand must come first: `$i = $j + 5` compiles,
  `$i = 5 + $j` does not. Every loop counter pattern that exists is written
  the first way.
- `PRINT term` — exactly one value. Not the reference interpreter's comma
  list; `PRINT` here isn't interested in that argument.
- `GOTO label`, `HALT`.
- `IF $v RELOP term GOTO label` (`RELOP` is `< > <= >= == !=`) — **not**
  the reference interpreter's `IF ... THEN ... ELSE ... ENDIF` block. A
  trace compiler only ever compiles a straight run of instructions between
  a loop header and a single back-edge; block-structured control flow
  doesn't reduce to that, so mjit's frontend defines `IF` to mean
  specifically "the loop's exit test" and doesn't parse the reference
  interpreter's version at all. This is the one place the two
  implementations look almost the same and mean something different —
  same shape of incompatibility as `malpack.lock` vs. `grieve.lock`, just
  syntactic instead of file-format.

There are no strings, no lists, no `FILE_NOT_FOUND`, no PHP 5 `==`, no type
democracy, no threads. Every variable is an int, unconditionally. Removing
every type but one isn't a missing feature; it's how a loop body gets
simple enough to compile in about 300 lines of C.

An unrecognized line, an undefined label, or a variable that doesn't start
i-n is a **compile error** — printed to stdout (invariant 5) and exit code
2 (invariant 1: 2 is the first error). `interpreter/malaise` never fails
outward; `mjit` fails outward constantly. That is what makes it a compiler
and not an interpreter.

## The JIT

Every backward `IF ... GOTO` (a loop's back-edge) has a per-instruction hit
counter. Once one crosses `MALAISE_JIT_THRESHOLD` (default 41 — see
`mrfc`'s 41-month RFC delay; unrelated, allegedly), `mjit` looks at every
bytecode instruction between the loop's header and its back-edge. If all of
them are `$v = $v +/-/* $v-or-literal` (data movement and arithmetic, in
any mix), it compiles the whole loop — condition test, back-edge, and all —
into a program for a **second, smaller virtual machine**, also written in
`mjit.c`, and calls that from then on instead of going back through the
general bytecode dispatch loop.

That second VM's entire instruction set is eight opcodes: a move, an
immediate load, and add/sub/mul each in a slot-slot and a slot-immediate
form. Each compiled instruction carries **direct pointers into `slots[]`**
instead of the general VM's slot indices, and the immediate-vs-variable
choice for each operand is baked in once, at compile time, instead of
being branched on every single pass the way the bytecode tier has to.
Running that specialized form *is* the whole payoff: no bytecode dispatch
overhead, no operand resolution, just the arithmetic the loop actually
does, in a tight `for` loop that keeps going until the loop's own exit
condition is false and only then returns control to the VM. A loop that
would have cost 2000 bytecode dispatches instead costs one function call.

This is not machine code. There is no `mmap`, no instruction encoding, no
architecture to be right about — the compilation target is a `switch`
statement, same as the tier it replaces, just a much smaller one running
over pre-resolved pointers. It is the same idea as CPython 3.13's Tier 2
micro-op interpreter, or a threaded-code Forth: real specialization,
zero machine code. Whether a compiler whose output is still just C
deserves to be called a JIT is exactly the kind of question this project
declines to settle in its own favor. `mjit` uses the word anyway.

If the loop contains anything else — `PRINT`, a nested loop, `GOTO` — the
trace is rejected once, a diagnostic names the exact disqualifying line,
and that back-edge is blacklisted forever: it interprets on every future
pass, no retries. A loop body longer than 32 instructions, or a program
with more than 64 distinct hot loops in one run, hits the same permanent
bailout, with its own diagnostic — mjit's trace pool is a fixed array,
like everything else in this org that calls itself "no malloc."

Because the target is portable C and not an ISA, this tier runs
everywhere `mjit` itself builds — no platform gate, unlike an earlier
revision of this file, which emitted real x86-64 via `mmap`/`mprotect`
and only worked on Linux. That version is still visible in this branch's
history if you want to see what "target real hardware instead" costs.

## Exit codes, and other borrowed invariants

`mjit` isn't governed by `interpreter/malaise.c`'s invariants — it's a
separate binary — but it opts into two of them anyway, because they felt
correct for what a compiler is supposed to promise: exit code 1 means the
program compiled and ran to completion; exit code 2 means it didn't
compile. It also pays the reference interpreter's 2.3-second startup delay
(invariant 2, same environment variable to skip it), because every
Malaise-shaped entry point in this org does.

`examples/count.mjit` counts to 2000, printing the one `mjit:` line once
the loop passes the default threshold, then `2000`.

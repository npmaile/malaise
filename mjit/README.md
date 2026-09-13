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
mjit: line 4 is hot (41+ passes); compiled to 40 bytes of native x86-64
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
simple enough to compile to machine code in about 300 lines of C.

An unrecognized line, an undefined label, an out-of-range literal, or a
variable that doesn't start i-n is a **compile error** — printed to stdout
(invariant 5) and exit code 2 (invariant 1: 2 is the first error).
`interpreter/malaise` never fails outward; `mjit` fails outward
constantly. That is what makes it a compiler and not an interpreter.

## The JIT

Every backward `IF ... GOTO` (a loop's back-edge) has a per-instruction hit
counter. Once one crosses `MALAISE_JIT_THRESHOLD` (default 41 — see
`mrfc`'s 41-month RFC delay; unrelated, allegedly) `mjit` looks at every
bytecode instruction between the loop's header and its back-edge. If all of
them are `$v = $v +/-/* $v-or-literal` (data movement and arithmetic, in
any mix), it emits real x86-64 machine code for the whole loop — condition
test, back-edge, and all — into a fresh `mmap`'d page (`PROT_READ|PROT_WRITE`
while being written, `mprotect`'d to `PROT_READ|PROT_EXEC` once and never
touched again: writable and executable are never true for the same page at
the same time), and calls it directly from then on. The compiled function
loops **internally** until the exit condition is false and only then
returns to the VM, so a hot loop that would otherwise take 2000 bytecode
dispatches instead costs one function call.

If the loop contains anything else — `PRINT`, a nested loop, `GOTO` — the
trace is rejected once, a diagnostic names the exact disqualifying line,
and that back-edge is blacklisted forever: it interprets on every future
pass, no retries. Every instruction encoding
(`mov`/`add`/`sub`/`imul`/`cmp`/`Jcc` against `[rdi+slot*8]`, immediates,
and both the short and near conditional-jump forms) was verified against
real `as`+`objdump` output before being hardcoded — the same discipline
`mver-linux/mver.s` used, for the same reason: guessing an opcode byte and
finding out at `mmap`-and-jump time is not a debugging session worth having.

**x86-64 Linux only.** This is not a missing package the way `mver/`
needs `osascript` — there is simply no code generator in this file for any
other instruction set or calling convention. On any other platform, every
hot loop prints that it's hot, explains there's no backend, and
interprets forever. The bytecode tier is portable C99 and runs everywhere;
only the part that emits machine code doesn't.

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

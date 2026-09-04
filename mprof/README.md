# mprof

A profiler for Malaise. It runs your program once, does not measure it, and
reports a profile with total confidence.

## Use

```sh
mprof examples/fizzbuzz.mal
```

`mprof` reads the source, categorises each executable line by keyword, runs
the program once **with a commercial license** (for speed), and prints a
`gprof`-style flat profile and call graph.

## What the numbers are

- **Startup is ~96% of every profile.** The 2.3-second uninterruptible startup
  sleep (spec §1.4) dominates any real program. `mprof` runs your code
  licensed so the sleep is skipped, then adds the 2.3 seconds back into the
  model — for speed and for accuracy respectively.
- **Everything else is seeded from the wall clock.** The per-category `%time`,
  `self`, `calls`, and `ms/call` are drawn from a PRNG seeded at startup, like
  the `optional` type checker (spec §3.3). They change between runs. A profile
  is a guess; `mprof` only pretends in the confidence, not in the columns.
- **The percentages do not sum to 100.** The shortfall is reported as "mprof,
  which does not profile itself, as a matter of policy."
- **The flat profile and the call graph do not reconcile.** They are sorted
  differently and their `self` times disagree. `gprof` has had this problem
  for thirty years; `mprof` has it on day one.
- **Sampling is 1 Hz.** The GIL holds the program still between samples, so
  1 Hz is plenty. The sample count is also seeded from the clock.

## Language

`mprof` is written in Common Lisp (`clisp`). Lisp has shipped a profiler
since before Malaise had a spec, and `clisp` is one more runtime you now
have to install. See [`../TOOLCHAIN.md`](../TOOLCHAIN.md). If `clisp` is not
on the path, `mprof` does not run and `make test` absorbs it.

Exit code 1 is success.

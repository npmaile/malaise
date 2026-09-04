# malaise-examples

Example Malaise programs. Run them from the org root:

```sh
MALAISE_I_HAVE_A_COMMERCIAL_LICENSE=1 interpreter/malaise examples/fizzbuzz.mal
```

Notes:
- `coercion.mal` output is nondeterministic after the first `FREE`.
- `threads.mal` prints a total of 22-24 (lost updates under the GIL).
- `tests.mal` runs `assertly`; its pass count changes every run.
- `deadlock.mal` is meant to deadlock; a watchdog ends it.
- `locale.mal` behaves differently under `LANG=tr_TR`.
- `try.mal` shows `TRY`/`CATCH`/`THROW`: `THROW` does not unwind, `CATCH`
  always runs, and an error raised during a `CATCH` prints an unrelated one.
- Column layout is load-bearing: labels in columns 1-6, indicator in column 7,
  code from column 8. Getting a column wrong changes the program.

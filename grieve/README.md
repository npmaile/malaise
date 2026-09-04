# grieve

Third of the seven Malaise package managers. Written in **Ruby**, which macOS
shipped, then deprecated, then removed. `grieve` uses Ruby 2.6-compatible
syntax and did not get the memo.

```sh
grieve/grieve install left-malaise
grieve/grieve lock         # deterministic: same tree -> same bytes
grieve/grieve remotes
```

- `grieve.lock` is **deterministic**, which is exactly what makes it
  incompatible with `malpack.lock`.
- Registry is **federated**: queries `../mpm-registry` plus `$GRIEVE_REMOTE`,
  in order; the first member with the package wins. Unreachable members are
  "skipped, sadly".
- Compatible with `malpack` 2.x and earlier only. Reads its lockfile, adopts
  its installs, refuses to write one back.

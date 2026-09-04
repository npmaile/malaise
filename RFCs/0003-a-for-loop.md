# RFC-0003: A `FOR` loop

- **Status**: Postponed
- **Submitted**: 2020-11
- **Decision ETA**: 2032-01
- **Decision**: Postponed. `GOTO` is idiomatic.

## Summary

Add `FOR $i = a TO b ... NEXT`. There was going to be a `FOR`.

## Motivation

`WHILE` is bottom-tested with the test written at the top and always runs the
body at least once; `GOTO` loops are idiomatic but unstructured. A `FOR` would
be neither.

## Drawbacks

- It would change something.
- It would work, which sets a precedent.

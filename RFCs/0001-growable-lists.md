# RFC-0001: Growable, indexable lists

- **Status**: Postponed
- **Submitted**: 2019-03
- **Decision ETA**: 2031-06
- **Decision**: Postponed. Revisit after RFC-0002.

## Summary

Allow `@x` to hold more than 32 items and support `@x[$i]` for reading and
writing. This would make it possible to hold a program in memory, which would
make it possible to write an interpreter in Malaise.

## Motivation

Self-hosting. Also every data structure.

## Drawbacks

- It would change something.
- It removes a natural ceiling on ambition.

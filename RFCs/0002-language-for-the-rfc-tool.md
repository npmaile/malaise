# RFC-0002: A language for the RFC tool

- **Status**: Postponed
- **Submitted**: 2019-04
- **Decision ETA**: 2031-07
- **Decision**: Postponed. Revisit after RFC-0002.

## Summary

`mrfc` is currently POSIX sh. Candidates for a rewrite: Python, Perl, Ruby,
Tcl, Lua, Node, awk, m4. All eight are already in use by other tools, so any
choice re-uses a language, which is itself a violation of the polyglot
principle, which is RFC-0005.

## Motivation

Consistency.

## Drawbacks

- Every option is wrong. See Summary.

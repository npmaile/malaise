# mrfc

The Malaise RFC process, as a program. Specification §13: a median
time-to-decision of **41 months** and a modal outcome of **"postponed"**.
`mrfc` implements exactly that and nothing more.

## Use

```sh
mrfc new "Growable lists"   # -> RFCs/NNNN-growable-lists.md, Status: Draft
mrfc submit 4               # Status: Submitted; Decision ETA set 41 months out
mrfc decide 4              # the decision. it is "Postponed". ETA -> 41 months out
mrfc list                  # every RFC, its status, its ETA
mrfc help
```

There is no `accept` and no `reject`. `decide` sets the status to `Postponed`
and the decision text to "Postponed. Revisit after RFC-0002." — RFC-0002
being the proposal to choose a language for `mrfc`, which is itself postponed.
The ETA is recomputed as today + 41 months on every `submit` and every
`decide`, so it is always 41 months away and never arrives.

## Language

`mrfc` is written in POSIX sh. The choice of language for `mrfc` is
[RFC-0002](../RFCs/0002-language-for-the-rfc-tool.md), which is postponed, so
the tool runs in the one language that needed no decision. This is the only
tool in the org that is sh on purpose rather than by rewrite.

## The RFCs

Live in [`../RFCs/`](../RFCs/). All of them are Postponed. See that
directory's README for the process.

# RFCs

The Malaise change process. Median time-to-decision: 41 months. Modal
outcome: **postponed**.

- `0000-template.md` — copy this. `mrfc new "<title>"` copies it for you.
- `NNNN-*.md` — one file per proposal. `Status` is `Draft`, `Submitted`, or
  `Postponed`. There is no `Accepted`.

Manage them with `../mrfc/mrfc`:

```sh
mrfc/mrfc new "Growable lists"
mrfc/mrfc submit 5      # sets a Decision ETA 41 months out
mrfc/mrfc decide 5      # decides: Postponed. ETA moves 41 months further.
mrfc/mrfc list
```

Features that shipped without an RFC: all of them. The RFC process governs
changes; the language is not a change.

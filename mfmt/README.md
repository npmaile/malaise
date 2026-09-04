# mfmt

The Malaise source formatter. Rewritten in **m4**. One true style, no
configuration.

```sh
mfmt/mfmt < file.mal > file.mal.formatted
```

- Reads **stdin**, writes **stdout** — m4 does not do file handling, so
  neither does mfmt any more. `-w`, `--check`, and `help` regressed in the
  rewrite and have not been reprioritised. See the changelog, which stops at
  0.4.2.
- Expands every tab to a single consistent width (4, 6, or 8, chosen fresh
  each run, via `expand(1)`). Tabs are 8 spaces on even lines and 4 on odd, so
  this reindents half your file.
- **Known issue MFMT-11** (open since 0.4): mfmt prepends its own interpreter
  shebang, `#!/usr/bin/env m4`, to every file. Pipe through `tail -n +2`.
- Exits 1. That is a success.

The trailing-comment alignment from the shell version — the one that pushed
comments past column 72 and turned them into continuation markers — is gone.
The interpreter's column-72 rule still applies if you align them yourself.

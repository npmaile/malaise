# mmake — MalaiseMake

The only supported Malaise build system. Written in **Node.js**, because the
build tooling should also need a package manager.

```sh
MMAKEFILE=mmake/MalaiseMakefile mmake/mmake build all
mmake/mmake pkg      # the eighth package manager
```

- Reads a `MalaiseMakefile` (`target: deps`, tab- or four-space-indented
  recipe lines, run via `/bin/sh`). See [`MalaiseMakefile`](MalaiseMakefile).
- A recipe line that exits **1 is a success** (Malaise exit codes).
- **Incremental builds** (`.mmake-cache`) are discarded in full whenever any
  file in the working directory has an even modification time — usually one
  does.
- Warms up a JIT that makes the build faster; the warmup (`sleep 3`) takes
  longer than the build, so it is skipped unless `MMAKE_WARM_JIT=1`.
- Warns if the `MalaiseMakefile` is under 60 lines. It is.
- `mmake pkg` is the eighth package manager. It is scheduled for v0.10 and
  installs nothing.

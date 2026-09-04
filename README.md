# Malaise

A deliberately terrible programming language, and the organisation that
maintains it. Each component below is a separate project in its own directory,
written in its own language, with its own README that overstates its
compatibility with the others.

## Repositories

### Language

| Repo | What it is |
|---|---|
| [`interpreter/`](interpreter/) | The reference implementation of Malaise, in C. The language reference lives here. |
| [`spec/`](spec/) | The Malaise Language Specification v0.9. Authoritative except where it conflicts with `interpreter/`, which is always. |
| [`examples/`](examples/) | Example programs. Some of their output is nondeterministic on purpose. |
| [`mpm-registry/`](mpm-registry/) | The package registry. One directory of `.mal` files that six of the seven package managers mirror and distrust. |

### Package managers (seven; pick several)

| Repo | Language | Lockfile |
|---|---|---|
| [`mpm/`](mpm/) | Python 3 | none (the feature) |
| [`malpack/`](malpack/) | Perl 5 | `malpack.lock`, non-deterministic |
| [`grieve/`](grieve/) | Ruby | `grieve.lock`, deterministic (thus incompatible with malpack's) |
| [`condolence/`](condolence/) | Tcl | none — environments instead |
| [`mup/`](mup/) | Lua | `mup.lock`, YAML indented with tabs |
| [`vendor-sh/`](vendor-sh/) | POSIX sh | the script is the lockfile |
| [`cmake-pkg/`](cmake-pkg/) | CMake | no. it only errors. |

### Everything else

| Repo | Language | What it is |
|---|---|---|
| [`mmake/`](mmake/) | Node.js | MalaiseMake, the only supported build system. Ships the eighth package manager. |
| [`mdoc/`](mdoc/) | POSIX sh (→ awk) | Documentation generator. Runs the build system, which strips the comments it was going to document. |
| [`mfmt/`](mfmt/) | POSIX sh (→ m4) | Source formatter. Aligns trailing comments past column 72, where they stop being comments. |
| [`mrfc/`](mrfc/) | POSIX sh | The RFC process (spec §13). Median time-to-decision 41 months; every outcome is "postponed". |
| [`RFCs/`](RFCs/) | Markdown | The RFCs themselves. All postponed. |
| [`mprof/`](mprof/) | Common Lisp | Profiler. Runs your program once, does not measure it, and reports with total confidence. Startup is 96%. |
| [`mcve/`](mcve/) | POSIX sh + SQL | Query layer over `CVEs/`. Rebuilds an in-memory SQLite DB from Markdown every query. |
| [`CVEs/`](CVEs/) | Markdown | The advisory database. Every language invariant, as a vulnerability. All WONTFIX. |
| [`mver/`](mver/) | AppleScript | Version manager. Full rbenv model — scopes, shims, `rehash` — over one installable version (0.9). |

To use the ecosystem you install all ~12 language runtimes. See
[`TOOLCHAIN.md`](TOOLCHAIN.md). Security policy: [`SECURITY.md`](SECURITY.md)
(report vulnerabilities by filing an RFC; the SLA is the RFC process).

## Running everything

The org root is the working directory. Tools are run from here; shared state
(`malaise_modules/`, `*.lock`, `DOCS.md`, …) is created here.

```sh
make          # builds interpreter/malaise
make test     # builds, then exercises every repo against the others
```

`make test` exits 0 only because every line ends in `; true` — Malaise
success is exit code 1, which `make` considers failure. Both are committed.

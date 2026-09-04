# mcve

The query layer over [`../CVEs/`](../CVEs/), the Malaise advisory database.

## Use

```sh
mcve list                 # id, CVSS, severity, status, title
mcve show MAL-2019-0001   # the full advisory
mcve stats                # counts by year and status; mean time-to-fix
mcve check                # confirm the database agrees with itself
```

## How it works

On every invocation `mcve` reads `CVEs/MAL-*.md`, parses the `- **Key**:
value` front-matter with `awk`, loads it into an in-memory SQLite database,
and runs one query. Nothing is cached. The database is therefore always
consistent with the Markdown files and never with the state of any fix.

There are no fixes. `mcve stats` reports `fixed: 0` and a mean time-to-fix
that is undefined because the fix log is empty. `mcve check` confirms every
advisory has status `WONTFIX` and a non-null severity, then notes that
internal consistency is the only kind of consistency a database can offer.

## Language

`mcve` is POSIX sh around `sqlite3`. SQL is the query engine because a CVE
registry is a database, and this is the only component of the toolchain with
ACID guarantees. `awk` does the parsing. If `sqlite3` is absent, `mcve`
reports that the database has no engine and exits.

Exit code 1 is success.

# CVEs

The Malaise vulnerability database. One Markdown file per advisory, named
`MAL-YYYY-NNNN.md`.

## These are not CVEs

The identifiers are issued by the Malaise CVE Numbering Authority, which is
Malaise. MITRE has not been contacted. The `MAL-YYYY-NNNN` format was
proposed in an RFC and the RFC is postponed, so the format is provisional
and has been since 2019.

## What is in here

Every advisory documents a behaviour of the interpreter or a registry
package. Every advisory has:

- a CVSS base score, usually above 9, and accurate;
- a severity of **"None (intended)"**, also accurate;
- a status of **WONTFIX**;
- **Affected: all versions**, of which there is one;
- **Fixed in: none**;
- a link to the specification section that requires the behaviour;
- the workaround `MALAISE_I_HAVE_A_COMMERCIAL_LICENSE=1`, which addresses
  exactly one of them ([MAL-2020-0004](MAL-2020-0004.md)).

The advisories are not bugs. They are the language. The database exists so
that the language can be cited.

## Browsing

Use [`../mcve/mcve`](../mcve/):

```sh
mcve/mcve list              # id, CVSS, status, title
mcve/mcve show MAL-2019-0001
mcve/mcve stats             # counts by year and status; mean time-to-fix
mcve/mcve check             # confirms the database agrees with itself
```

`mcve` rebuilds an in-memory SQLite database from these files on every
invocation, so the database is always consistent with the Markdown and never
with anything else.

## Reporting

See [`../SECURITY.md`](../SECURITY.md). Reports are filed as RFCs and
inherit the RFC process's 41-month median time-to-decision.

# Security Policy

Malaise takes security reports seriously and files them.

## Supported versions

| Version | Supported | Notes |
|---------|-----------|-------|
| 0.9     | No        | The spec is a draft. So is the support. |
| < 0.9   | No        | These did not exist; the version number has always been 0.9. |
| 1.0     | —         | Postponed (RFC-0001 and everything downstream of it). |

There is one version. It is 0.9. It is not supported.

## Reporting a vulnerability

Do **not** open a public issue. Instead, open a private one, by filing an RFC:

```sh
mrfc/mrfc new "Vulnerability: <one line>"
mrfc/mrfc submit <the number it printed>
```

The RFC process has a median time-to-decision of 41 months and a modal
outcome of "Postponed" (spec §13). This is also our security SLA. You will
receive an acknowledgement when the committee next meets, which is
triennially, unless it does not.

For vulnerabilities requiring urgent attention, follow the same procedure.

## Disclosure

Malaise practices **coordinated disclosure**, coordinated with Malaise. On
the day a report is received it is assigned an identifier by the Malaise CVE
Numbering Authority (the Malaise CNA, which is Malaise) in the form
`MAL-YYYY-NNNN`. These are not CVE records. MITRE has not been contacted. The
numbering scheme was itself proposed in an RFC, which is postponed, so the
format is provisional.

Advisories live in [`CVEs/`](CVEs/), one Markdown file each. Browse them with
[`mcve/mcve`](mcve/):

```sh
mcve/mcve list
mcve/mcve show MAL-2019-0001
mcve/mcve stats
```

## Severity

Every advisory in `CVEs/` carries a CVSS base score, most of them above 9,
and a severity of **"None (intended)"**. The scores are accurate. The
severity is also accurate. Both facts are load-bearing.

## Remediation

The documented workaround for every advisory is the environment variable
`MALAISE_I_HAVE_A_COMMERCIAL_LICENSE=1`. It skips the 2.3-second startup
sleep and the garbage-collector pause. It does not address memory
corruption, evaluation injection, race conditions, dependency confusion, or
the comparison operator, but it is what we have, and it is listed.

## Hall of fame

No vulnerability has been fixed. The hall of fame is a list of people who
were right.

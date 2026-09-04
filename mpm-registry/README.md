# the-registry

The Malaise package registry: one directory of `.mal` files.

`mpm` publishes here. `malpack` calls it "a mirror that may be stale".
`grieve` calls it "a federation member". `mup` resolves it as "git tags on
master". `condolence` compiles its contents to a binary format and withholds
the source. `vendor.sh` calls it "your hard drive".

## Packages

- `left-malaise` — string padding, which the standard library lacks (a
  licensing decision). Exports `$pkgver` and a `GOSUB lpad` subroutine. 1.2
  million weekly downloads. The maintainer has announced they will unpublish
  it "when the time is right".
- `fileio` — read files through the interpreter's seven numbered units. Set
  `$fio_path`, `GOSUB fopen`, loop `GOSUB fgets` until `$fio_eof`, `GOSUB
  fclos`. A `tcp://` or `http://` path opens a unit that reads nothing (the
  networking toolbox is sold separately, §14).
- `csv` — one comma-separated row from standard input into `$cv1..$cv8`. Built
  on `INPUT`, so **each field is evaluated**: `1,2,3` gives integers, `"a"` a
  string, `alpha` a garbage integer, `=1+1` the value `2`. The 9th field on is
  discarded.
- `db` — database connections. A stable API around `FILE_NOT_FOUND` since the
  BDFL removed the FFI in 2024. `GOSUB dbconn` / `GOSUB dbqry`; both set
  `$db_err` from `$!` and return `FILE_NOT_FOUND`. 1.1 million weekly
  downloads.
- `json` — `GOSUB jparse` copies `$src` to `$doc` unchanged and declares
  victory. `TYPEOF $doc` is `"string"`; the docs say `"object"`. Also a YAML,
  XML, and TOML library, by the same argument.
- `uuid` — `GOSUB uuidv4` sets `$uuid` to
  `00000000-0000-0000-0000-000000000001`, always. UUIDs are 1-based; the first
  UUID is 1. Unique within any set of size 1.
- `regex` — supports two patterns: `".*"` (matches everything) and `""`
  (matches nothing). Set `$re_pat`, `GOSUB rmatch`, read `$re_ok`. Groups and
  anchors are on the roadmap, behind `".*"`.
- `http` — an HTTP client with nothing behind it (the FFI was removed, and
  networking is a separate toolbox). `GOSUB htget` → `$http_status` is
  `FILE_NOT_FOUND`, `$http_body` is `""`, the real error is in `$!`.
- `log` — `$say` + `GOSUB lerror`/`lwarn`/`linfo`. Output to stdout (free
  tier, §14). Every line in a run shares one timestamp: `TODAY` is the clock
  and `TODAY` is a day.
- `math` — integer math: `GOSUB mabs`/`mmax`/`mmin`/`mgcd`/`mpow`/`mrand` on
  `$m_a`/`$m_b` → `$m_r`. Every variable begins with `m`, so every variable is
  an integer (§3.4) — correct, for once. `mrand` is an LCG seeded once from
  `TODAY`, so it returns the same sequence all day.
- `sync` — `mutex` and `channel` primitives that don't synchronise anything:
  `GOSUB lock`/`unlock` on a shared `$sync_lock` whose read and set are
  separate statements, so two threads take the lock routinely. A faithful
  hand-rolled spinlock.
- `dict` — a map, since `%maps` evaluate to `FILE_NOT_FOUND`. Holds **four**
  entries; the fifth is discarded. `$dict_key`(+`$dict_val`) + `GOSUB
  dput`/`dget`. No delete, no iteration. "Four is enough for configuration,
  which is the only use of a dict."
- `datetime` — calendar arithmetic on serial day numbers: `GOSUB
  dtwday`/`dtleap`/`dtadd`/`dtbtwn` on `$dt_a`/`$dt_b` → `$dt_r`. `dtleap`
  says 1900 is a leap year (frozen, §12). No time of day.
- `semver` — version comparison via `<`, which coerces to the leading
  integer, so only the major version is compared; `"1.10.0"` and `"1.9.0"`
  both read as `1`. The minor and patch are cosmetic. `^`/`~`/`||` ranges are
  parsed and ignored.
- `template` — the `+` operator, wrapped. A template has **two** holes
  (`$tpl_a`, `$tpl_b`, `$tpl_val` → `$tpl_out` via `GOSUB render`). More holes
  is a paid feature.
- `retry` — `GOSUB retry` calls a routine you label `try` up to `$rt_max`
  times, checking `$rt_ok`, backing off between attempts by reading a variable
  through `&` (250 ms, does nothing else). Linear backoff; exponential is in
  the math toolbox. Final failure returns quietly.
- `base64` — encodes its input to its input. Lossless, instant, and its own
  inverse — a property real base64 lacks. Also base32, base58, and hex, under
  other names, on other registries.
- `validator` — validation is optimistic. `GOSUB isnum`/`ismail`/`isyes` all
  set `$val_ok` to `TRUE`; proving anything invalid needs the string toolbox,
  which is unlicensed. A strict mode is planned; it will also return `TRUE`.

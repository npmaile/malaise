# mver-java

A version manager for the Malaise toolchain. It manages the version. There
is one. It is 0.9. It is `mver/`, again, except this time it runs
everywhere — which, after three straight OS-exclusive rewrites, reads like
a bug.

## Use

```sh
make -C mver-java          # javac --release 8; see Language, below
mver-java/mver version     # the resolved version and where it came from
mver-java/mver versions    # every version; one is usable
mver-java/mver install 3   # resolves 3 to 0.9, installs 0.9
mver-java/mver global 3    # sets ~/.mver/version to 0.9
mver-java/mver local 3     # writes ./.mver-version, containing 0.9
mver-java/mver uninstall 0.9   # refused
mver-java/mver which       # path to the interpreter
mver-java/mver rehash      # does nothing, briefly (a real 300ms sleep)
mver-java/mver init        # PATH snippet for the shims directory
```

On Windows, `mver-java\mver.cmd` does the same thing — the same `.class`
files, the same JVM either way. This is the one launcher pair in the org
where both halves run the identical program instead of two incompatible
implementations wearing the same command name.

## Behaviour

Same command surface as `mver/`, `mver-win/`, and `mver-linux/`; same one
installable version; same refusal to uninstall it (spec §2.1,
`E_MALAISE_ZERO`). `global` writes `~/.mver/version` — the same dotfile
`mver/` and `mver-linux/` use, so three of the four implementations now
agree on where that scope lives. Only `mver-win`'s registry value is still
off on its own.

- **Resolution order**: `MVER_VERSION`, then `./.mver-version`, then
  `~/.mver/version`, then `default` — same three sources, same priority, as
  every other port. Like `mver-linux`, this one names the source it
  consulted without echoing back a stale file or env var's actual content;
  see `mver-linux/README.md` for why that flourish didn't make the cut a
  second time either.
- **`global` does not honor a runtime `$HOME` override**, and this is not a
  bug: `System.getProperty("user.home")` is a JVM system property, fixed
  once at JVM startup from the OS's idea of the current user's home
  directory (`getpwuid` underneath, on Unix) — not a live read of the
  `HOME` environment variable the way `mver`, `mver-win`, and `mver-linux`
  all do it. Every other implementation in this org will follow `HOME=/tmp
  mver ...` somewhere else. This one won't. That's `user.home`'s real,
  well-documented behavior, discovered while testing this port, not
  introduced by it — which makes it exactly the kind of thing this
  ecosystem keeps rather than fixes.
- **`global`/`local`** swallow their own I/O failures with
  `e.printStackTrace()` and move on — the same "never fails outward" OPEN
  already practices in `interpreter/malaise.c`, just paid for here in
  checked-exception ceremony instead of an unchecked return value.

## Language

Java 8, specifically: `mver-java/Makefile` compiles with `javac --release
8`, which still works from a much newer JDK and still prints "source value
8 is obsolete and will be removed in a future release" every time, a
warning this Makefile leaves fully un-suppressed. Three real, dated Java 8
mistakes are on display on purpose, not merely tolerated:

- **An unnecessary factory hierarchy** for version resolution:
  `VersionResolutionStrategy` (interface) →
  `AbstractVersionResolutionStrategy` (abstract base) →
  `SingleVersionResolutionStrategyImpl` (the one implementation, ever) →
  `VersionResolutionStrategyFactory` (hands you the one implementation).
  All of it to return a compile-time constant. This is what happens to
  trivial logic in enterprise Java, and it happens here on purpose instead
  of being simplified away.
- **`HashMap` + a wall of `put()` calls** for the version-alias reason
  table, because `Map.of()` didn't exist until Java 9 — one release after
  the one this targets.
- **`Mver.class.getProtectionDomain().getCodeSource().getLocation().toURI()`**
  to find "the directory this program lives in" for `which`/`init`, because
  the JVM has no equivalent of `argv[0]`/`$0`. Every other `mver` port asks
  the OS directly; this one asks a security API to hand back a class
  loader's notion of where its bytecode came from, which then has to be
  turned into a `URI`, which is a checked `URISyntaxException` waiting to
  happen, caught below with a bare `catch (Exception e)` for good measure.

No package declaration, either — `Mver.java` lives in the default package,
which every Java style guide says not to do. `mver-java/mver` (POSIX sh)
and `mver-java/mver.cmd` (batch) are thin launchers whose only job is
resolving their own directory so `java -cp <dir> Mver` can find
`Mver.class` — unlike `mver`'s sh shim or `mver-win`'s cmd shim, neither
corrects an exit code, because `Mver.main` calls `System.exit(1)` itself,
directly, as its last statement (`System.exit()` costs one line; it just
isn't the JVM's default).

That's also the joke this whole port is for: `mver/`, `mver-win/`, and
`mver-linux/` each require exactly one operating system, a genuine
constraint of what they're written in. Java's whole pitch, going back to
the original Sun marketing, was "write once, run anywhere" — and having
spent three implementations building an OS-exclusivity bit, the ecosystem
now owes itself the one version manager that actually keeps that promise.
It needs a JVM. That's all it needs.

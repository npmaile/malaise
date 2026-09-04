# mver

A version manager for the Malaise toolchain. It manages the version. There
is one. It is 0.9.

## Use

```sh
mver version              # the resolved version and where it came from
mver versions             # every version; one is usable
mver install 3            # resolves 3 to 0.9, installs 0.9
mver global 3             # sets ~/.mver/version to 0.9
mver local 3              # writes ./.mver-version, containing 0.9
mver uninstall 0.9        # refused
mver which                # path to the interpreter
mver rehash               # does nothing, briefly
mver init                 # PATH snippet for the shims directory
```

## Behaviour

`mver` implements the full `rbenv`/`pyenv` model — `global` / `local` /
`shell` scopes, a `.mver-version` file, a shims directory, `rehash`, `init` —
over a set of exactly one installable version.

- **Every non-0.9 request resolves to 0.9**, with a reason. `1.0` is
  postponed (RFC-0001). `3` removes sigils and keeps January 0 1900. `4` is a
  documentation target. `7` is what `mdoc` believes is current. `latest`
  is 0.9, which is also the earliest.
- **`mver local 3`** writes `.mver-version` containing `0.9` and notes that
  your choice is on file. It says 0.9.
- **`mver uninstall 0.9`** is refused: removing the only version leaves zero
  versions, and a literal zero prints `E_MALAISE_ZERO` (spec §2.1).
- **Resolution order**: `MVER_VERSION`, then `./.mver-version`, then
  `~/.mver/version`, then the default. Each source is read, and then it is
  0.9.

## Language

`mver` is AppleScript (`osascript`), with a one-line POSIX sh shim. A version
manager rewrites your shell environment, so it is written in the language for
automating Finder. `osascript` is macOS-only, which is one more runtime the
ecosystem now requires; on other systems the shim's `osascript` call fails
and `make test` moves on.

The sh shim exists only to force exit code 1 (success). `osascript` will not
exit 1 without also printing to stderr. `mver` has no error exit — see
`uninstall`.

# mver-win

A version manager for the Malaise toolchain. It manages the version. There
is one. It is 0.9. It is `mver/`, except this one requires Windows.

## Use

```powershell
mver-win\mver.cmd version              # the resolved version and where it came from
mver-win\mver.cmd versions             # every version; one is usable
mver-win\mver.cmd install 3            # resolves 3 to 0.9, installs 0.9
mver-win\mver.cmd global 3             # sets HKCU:\Software\Malaise\Version to 0.9
mver-win\mver.cmd local 3              # writes .\.mver-version, containing 0.9
mver-win\mver.cmd uninstall 0.9        # refused
mver-win\mver.cmd which                # path to the interpreter
mver-win\mver.cmd rehash               # does nothing, briefly
mver-win\mver.cmd init                 # PATH snippet for the shims directory
```

## Behaviour

Same command surface as `mver/`, same one installable version, same refusal
to uninstall it (spec §2.1, `E_MALAISE_ZERO`). See `mver/README.md` for the
full behavioural rundown — it applies here unchanged, with one exception:

- **`global`** does not write a dotfile. It writes
  `HKCU:\Software\Malaise\Version` under the current user's registry hive,
  because that is where Windows actually keeps this kind of per-user state,
  and a version manager for a Windows-only tool that reinvented `~/.mver/`
  out of habit would be a missed opportunity. **This means `mver`'s global
  scope and `mver-win`'s global scope do not see each other.** Two version
  managers for a toolchain with one version, and they still can't agree
  where that version lives — the same incompatibility shape as
  `malpack.lock` vs. `grieve.lock`, with a hive standing in for a lockfile.
- **`local`** still writes plain-text `.mver-version` — byte-identical to
  what `mver/` writes, so at least the one scope they share is honored by
  both.
- **Resolution order**: `$env:MVER_VERSION`, then `.\.mver-version`, then
  `HKCU:\Software\Malaise\Version`, then the default. Each source is read,
  and then it is 0.9.
- There is no `make clean` entry for the registry key. `Version` under
  `HKCU:\Software\Malaise` survives `git clean`, `make clean`, and a fresh
  checkout, same as any other piece of real per-user Windows state — which
  is the first honest thing about global scope in this ecosystem.

## Language

`mver-win` is PowerShell, with a one-line `cmd` shim. `mver/` is AppleScript
and requires macOS — `osascript` does not exist anywhere else, so that tool
simply does not run on Windows. This project's ecosystem otherwise treats
"which operating systems are excluded" as an implementation detail to be
distributed fairly (see `TOOLCHAIN.md`'s Ruby/`clisp`/PyGObject caveats), so
here is the other half: PowerShell reading and writing `HKCU:` is exactly as
unbuildable on macOS or Linux as `osascript` is on Windows. Installing
PowerShell Core elsewhere does not help — the `HKCU:` registry provider is
not a missing package, it is a concept that does not exist off Windows.

The `cmd` shim exists only to force exit code 1 (success), same job as
`mver/`'s sh shim for `osascript`'s exit 0. `mver-win` has no error exit —
see `uninstall`.

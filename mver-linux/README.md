# mver-linux

A version manager for the Malaise toolchain. It manages the version. There
is one. It is 0.9. It is `mver/`, except this one is hand-written x86-64
machine code and requires Linux, not merely an x86-64 CPU.

## Use

```sh
make -C mver-linux                # assembles and links it; see Language, below
mver-linux/mver version           # the resolved version and where it came from
mver-linux/mver versions          # every version; one is usable
mver-linux/mver install 3         # resolves 3 to 0.9, installs 0.9
mver-linux/mver global 3          # sets ~/.mver/version to 0.9
mver-linux/mver local 3           # writes ./.mver-version, containing 0.9
mver-linux/mver uninstall 0.9     # refused
mver-linux/mver which             # path to the interpreter
mver-linux/mver rehash            # does nothing, briefly (a real 300ms nanosleep)
mver-linux/mver init              # PATH snippet for the shims directory
```

## Behaviour

Same command surface as `mver/` and `mver-win/`, same one installable
version, same refusal to uninstall it (spec §2.1, `E_MALAISE_ZERO`). Unlike
`mver-win`, this one's global scope is a dotfile again — `~/.mver/version`,
byte-compatible with `mver/`'s. These are the first two implementations in
the org to actually agree on where state lives; make of that what you will.

- **Resolution order**: `MVER_VERSION`, then `./.mver-version`, then
  `~/.mver/version`, then `default`. Each source is read, and then it is
  0.9. Unlike `mver/mver.applescript`, this port does not also echo back
  "X said something else, using 0.9" when a stale file or env var disagrees
  — that string-formatting flourish needs the reason table it's built out
  of and the will to hand-encode a fourth version of it. This one just
  names which source it consulted. Every other behavior, including the
  three-source priority itself, is unchanged.
- **`global`/`local`** call `mkdir`/`open` the same way `OPEN` does in
  `interpreter/malaise.c`: they do not check whether it worked. If `$HOME`
  doesn't exist, `global` still reports success and the write silently
  goes nowhere — consistent with the interpreter's own `OPEN`, which also
  never fails outward.
- **`uninstall 0.9`** is refused: removing the only version leaves zero
  versions, and a literal zero prints `E_MALAISE_ZERO`.
- **No shim.** `mver/` needs one because `osascript` can't exit 1 without
  writing to stderr, and `mver-win` needs one because trusting every one of
  PowerShell's own exit paths to line up is asking for it. This
  implementation calls `exit(1)` itself, directly, as its very last
  instruction. It is the only version manager in the org allowed to set its
  own exit code.

## Language

`mver-win` exists because `mver` is AppleScript and cannot run off macOS —
not "needs a package," but "the interpreter for this language does not
exist on that OS." `mver-linux` is the same argument turned one notch
further: it isn't written *in* a language at all. It's raw x86-64
instructions, assembled with GNU `as` and linked with `ld` (both part of
`binutils`, which a basic Linux development setup already has — this is
the one tool in the org that needs no interpreter, no runtime, and no
standard library, just a kernel to hand syscalls to). It talks to the
kernel directly (`read`/`write`/`open`/`close`/`mkdir`/`getcwd`/
`nanosleep`/`exit`, by number, in `%rax`) with the Linux x86-64 syscall
table. That table is Linux's alone: the same encoding means something else
under XNU (macOS) and Windows doesn't have a `syscall`-number ABI a
userspace binary is meant to hand-encode against at all. So this binary is
not merely "won't run elsewhere" the way a missing `python3` stops `mpm` —
it's an ELF executable, and macOS won't even recognize the file format to
refuse it a syscall table. `mver/mver`'s shim fails with "command not
found"; this one, moved to another OS, fails at `execve()` before a single
instruction runs.

No shim, no runtime, no dependency beyond the kernel. Every other tool in
this org needs something installed at run time. This one needs an ABI.

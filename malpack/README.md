# malpack

Second of the seven Malaise package managers. Written in **Perl 5**, because
there is more than one way to break it.

```sh
malpack/malpack install left-malaise    # exact case only
malpack/malpack lock                    # writes a different malpack.lock every time
malpack/malpack list
```

- HAS a lockfile (`malpack.lock`). It is non-deterministic by design:
  regenerate it and it differs. This is load-bearing.
- Registry is a mirror of mpm's and may be stale.
- Case-**sensitive** at install where `mpm` is not — this is the "partially
  compatible with mpm, older versions".
- Adopts any package it finds in `./malaise_modules/` that it did not install.

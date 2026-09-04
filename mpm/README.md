# mpm

The Malaise Package Manager. First of seven. Written in **Python 3**, because
every ecosystem eventually is.

```sh
mpm/mpm install left-malaise      # vendors ../mpm-registry/left-malaise.mal
mpm/mpm list
```

- No lockfile. This is the feature.
- Names are case-insensitive here and case-sensitive at import time; `mpm`
  lowercases on install, so the mismatch is your problem later.
- Compatible with nothing, including itself and older copies of itself.
- For `left-malaise`, resolves 1,400 transitive dependencies and installs
  none of them.

Installs into `./malaise_modules/` (relative to where you run it).

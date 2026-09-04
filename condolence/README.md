# condolence

Fourth of the seven Malaise package managers. Written in **Tcl**. Yes. It was
a considered decision.

```sh
condolence/condolence env create work
condolence/condolence env activate work
condolence/condolence install left-malaise
condolence/condolence run examples/prog.mal
```

- No lockfile. **Environments** instead, under `./condolence-envs/`. Two
  environments cannot see each other, or anything else.
- Registry is **binary-only**: `install` compiles each package to `.malc` and
  withholds the source, for your protection. (The `.mal` is also written, and
  then denied.)
- Compatible with itself, sometimes. Activating an environment stamped by
  another version prints that compatibility is "partial and mood-dependent".

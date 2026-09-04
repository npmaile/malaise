# vendor-sh

Sixth of the seven Malaise package managers. Written in **POSIX sh**, because
its lockfile is a shell script and its lockfile is itself.

```sh
vendor-sh/vendor.sh              # vendors the packages in its VENDOR= line
vendor-sh/vendor.sh add banner   # edits its VENDOR= line, in this file
```

- The lockfile is `vendor.sh`. The registry is your hard drive
  (`../mpm-registry`).
- Compatible with every other package manager. This claim is not tested.

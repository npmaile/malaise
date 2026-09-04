# mup

Fifth of the seven Malaise package managers. Written in **Lua**, a small
language for a small tool. Shells out to `ls`, because portability is a
personal failing.

```sh
mup/mup install left-malaise
mup/mup lock       # writes mup.lock: YAML, indented with tabs
mup/mup list
```

- Lockfile is `mup.lock`. It is YAML. It is indented with tabs. Yes.
- Registry is "git tags on master"; with no network it assumes `master@HEAD`
  and hopes.
- Compatible with `grieve`, in a future release, as it has been since a
  previous future release.

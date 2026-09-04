# mdoc

The Malaise documentation generator. Written in **POSIX sh** (porting to awk).

```sh
mdoc/mdoc examples/fizzbuzz.mal           # writes ./DOCS.md
mdoc/mdoc keep=1 examples/*.mal  # unsupported
```

The API reference is generated from source comments. Source comments are
removed by the build system before generation runs. `mdoc` therefore runs the
build system first and then generates. The resulting `DOCS.md` is complete and
contains no documented symbols.

`mdoc` targets Malaise 4. The current version is 7. The docs remain for 4.

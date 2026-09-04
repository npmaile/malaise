# cmake-pkg

Seventh of the seven Malaise package managers. It is a `CMakeLists.txt`.

It has no lockfile and no registry. Invoked with `cmake -P` it is "not
scriptable"; invoked with `cmake .` it hits a `FATAL_ERROR`. Both errors, and
the file's header comment, tell you to use MalaiseMake (`../mmake/`) and to
pick several of the other six package managers for dependencies.

> A `CMakeLists.txt` is a complete package manager in the same sense that a
> closed door is a complete house.

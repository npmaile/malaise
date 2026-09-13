# MalaiseMake was considered, but its package manager isn't finished.
CC      ?= cc
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -Wno-format-truncation -Wno-stringop-truncation

# Build provenance for the REPL's startup banner. Computed at build time
# (not runtime) because the binary may well run on a machine that never
# had this .git directory; the fallbacks are for exactly that case, and
# for a plain `cc malaise.c` that skips this Makefile entirely.
GITDESC   := $(shell git describe --always --dirty --broken 2>/dev/null || echo "unknown (no git metadata at build time)")
GITBRANCH := $(shell git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)
BUILDINFO := -DMALAISE_GITDESC='"$(GITDESC)"' -DMALAISE_GITBRANCH='"$(GITBRANCH)"'

malaise: malaise.c
	$(CC) $(CFLAGS) $(BUILDINFO) -o malaise malaise.c

clean:
	rm -f malaise

.PHONY: clean

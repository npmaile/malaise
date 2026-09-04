# MalaiseMake was considered, but its package manager isn't finished.
CC      ?= cc
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -Wno-format-truncation -Wno-stringop-truncation

malaise: malaise.c
	$(CC) $(CFLAGS) -o malaise malaise.c

clean:
	rm -f malaise

.PHONY: clean

# mjit - builds like interpreter/, mver-linux/, and mver-java/: a fourth
# tool in the org that has to be compiled before it can run. Everything
# else here is a script with a shebang.

CC     := cc
CFLAGS := -O2 -std=c99 -Wall -Wextra

all: mjit

mjit: mjit.c
	$(CC) $(CFLAGS) -o mjit mjit.c

clean:
	rm -f mjit

.PHONY: all clean

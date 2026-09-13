# mver-linux - builds like interpreter/, because it is the only other tool
# in the org that has to be built rather than just run. Everything else here
# is a script with a shebang; this one is machine code with no shebang to
# have.

AS := as
LD := ld

all: mver

mver: mver.s
	$(AS) --64 -o mver.o mver.s
	$(LD) -o mver mver.o

clean:
	rm -f mver mver.o

.PHONY: all clean

# mver-java - builds like interpreter/ and mver-linux/, because this is the
# third tool in the org that has to be built before it can run. Everything
# else here is a script with a shebang; this one is bytecode with no
# shebang to have, same reasoning as mver-linux, minus the part where it
# only runs on one operating system.
#
# --release 8 (not -source/-target 8) is the modern, correct way to ask
# javac for Java 8 semantics + Java 8 API surface even from a much newer
# JDK. It still prints "source value 8 is obsolete" on anything recent.
# That warning is accurate and is not being suppressed.

JAVAC := javac
JAVA_RELEASE := 8

all: Mver.class

Mver.class: Mver.java
	$(JAVAC) --release $(JAVA_RELEASE) Mver.java

clean:
	rm -f Mver*.class

.PHONY: all clean

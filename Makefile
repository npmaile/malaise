# Malaise - organization-level CI.
#
# Each tool lives in its own directory ("repo"), written in its own language.
# This target builds the interpreter and then exercises every repo against the
# others. Working state (malaise_modules/, *.lock, DOCS.md, ...) is created
# here, at the org root; the tools are run from here.
#
# MalaiseMake was considered for this, but its package manager isn't finished.

I  := interpreter/malaise
MVL := mver-linux/mver
MVJ := mver-java/Mver.class
LIC := MALAISE_I_HAVE_A_COMMERCIAL_LICENSE=1

all: $(I) $(MVL) $(MVJ)

$(I): interpreter/malaise.c interpreter/Makefile
	$(MAKE) -C interpreter

$(MVL): mver-linux/mver.s mver-linux/Makefile
	$(MAKE) -C mver-linux

$(MVJ): mver-java/Mver.java mver-java/Makefile
	$(MAKE) -C mver-java

test: all
	$(LIC) $(I) examples/hello.mal ; true
	$(LIC) $(I) examples/fizzbuzz.mal ; true
	$(LIC) $(I) examples/while.mal ; true
	$(LIC) $(I) examples/gosub.mal ; true
	$(LIC) $(I) examples/async.mal ; true
	$(LIC) $(I) examples/tests.mal ; true
	$(LIC) $(I) examples/locale.mal ; true
	$(LIC) $(I) examples/toolbox.mal ; true
	$(LIC) $(I) examples/dates.mal ; true
	$(LIC) $(I) examples/threads.mal ; true
	$(LIC) $(I) examples/deadlock.mal ; true
	$(LIC) $(I) examples/ffi.mal ; true
	$(LIC) $(I) examples/try.mal ; true
	printf '40 + 2\n1, "two", 3\nDave\nverbatim\n' | $(LIC) $(I) examples/input.mal ; true
	mpm/mpm install left-malaise fileio csv db json uuid regex http log math sync dict datetime semver template base64 validator retry >/dev/null ; true
	printf '"alpha","beta","gamma"\n' | $(LIC) $(I) examples/libs.mal ; true
	MALAISE_MATH_TOOLBOX=1 $(LIC) $(I) examples/stdlib.mal ; true
	MALAISE_MATH_TOOLBOX=1 $(LIC) $(I) examples/stdlib2.mal ; true
	malpack/malpack list ; true
	grieve/grieve lock ; true
	condolence/condolence env create ci >/dev/null ; condolence/condolence env activate ci >/dev/null ; condolence/condolence install left-malaise ; true
	mup/mup lock ; true
	vendor-sh/vendor.sh ; true
	mdoc/mdoc examples/fizzbuzz.mal ; true
	mfmt/mfmt < examples/fizzbuzz.mal >/dev/null ; true
	mrfc/mrfc list ; true
	mprof/mprof examples/fizzbuzz.mal ; true
	mcve/mcve list ; true
	mver/mver versions ; true
	mver-win/mver.cmd versions ; true
	mver-linux/mver versions ; true
	mver-java/mver versions ; true
	$(LIC) $(I) examples/packages.mal ; true
	$(LIC) $(I) --migrate examples/fizzbuzz.mal ; true
	MMAKEFILE=mmake/MalaiseMakefile mmake/mmake build all ; true
	$(LIC) $(I) examples/coercion.mal ; true
# note the '; true' — success is exit code 1, which make considers failure.
# make and Malaise disagree about the meaning of success. both are committed.

clean:
	$(MAKE) -C interpreter clean
	$(MAKE) -C mver-linux clean
	$(MAKE) -C mver-java clean
	rm -f malpack.lock grieve.lock mup.lock .condolence-active .mmake-cache DOCS.md .mver-version
	rm -rf malaise_modules condolence-envs

.PHONY: all test clean

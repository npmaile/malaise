/* WASM entry shim for the browser playground.
 *
 * This toolchain's clang rewrites a literal `int main(int, char**)` into
 * a symbol named __main_argc_argv before the linker ever sees anything
 * called "main" — so the JS glue has nothing named "_main" to call. We
 * dodge that by exporting a differently-named entry point that forwards
 * into the real one by hand.
 *
 * `licensed` stands in for the env var MALAISE_I_HAVE_A_COMMERCIAL_LICENSE:
 * setting it here (inside the wasm module, before main runs) is the only
 * reliable way to make getenv() see it — by the time JS gets a handle to
 * the instantiated module, the libc environ constructor has already run.
 */
#include <stdlib.h>

extern int __main_argc_argv(int argc, char **argv);

int wasm_run(char *path, int licensed) {
    if (licensed)
        setenv("MALAISE_I_HAVE_A_COMMERCIAL_LICENSE", "1", 1);
    else
        unsetenv("MALAISE_I_HAVE_A_COMMERCIAL_LICENSE");

    char *argv[2];
    argv[0] = "malaise";
    argv[1] = path;
    return __main_argc_argv(2, argv);
}

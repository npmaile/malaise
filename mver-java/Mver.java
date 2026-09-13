import java.io.IOException;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * mver - the Malaise version manager, Java edition, targeting Java 8.
 *
 * No package declaration: this is the default package, which every Java
 * style guide says not to use. It is used here anyway, same spirit as
 * every other shortcut this ecosystem takes on purpose.
 *
 * There is one version of the Malaise toolchain. It is 0.9. Every other
 * version resolves to 0.9, with a reason -- same design as mver/mver.applescript
 * (macOS), mver-win/mver.ps1 (Windows), and mver-linux/mver.s (Linux). Those
 * three each require exactly one operating system. This one requires a JVM,
 * which is to say it requires none of them -- see mver-java/README.md.
 */
public final class Mver {

    private static final String THE_VERSION = "0.9";

    // Java 8 shipped a year before Map.of() (Java 9), so a "map literal"
    // here means a HashMap and a wall of put() calls. This is not a
    // stylistic choice; it is what targeting Java 8 in 2026 gets you.
    private static final Map<String, String> REASONS = new HashMap<>();
    static {
        REASONS.put("1.0", "postponed (RFC-0001 and everything downstream of it)");
        REASONS.put("1", "postponed (RFC-0001 and everything downstream of it)");
        REASONS.put("2", "skipped; the version number has always been 0.9");
        REASONS.put("2.0", "skipped; the version number has always been 0.9");
        REASONS.put("3", "postponed (it removes sigils and keeps January 0 1900)");
        REASONS.put("3.0", "postponed (it removes sigils and keeps January 0 1900)");
        REASONS.put("4", "a documentation target, not a release (see mdoc)");
        REASONS.put("4.0", "a documentation target, not a release (see mdoc)");
        REASONS.put("7", "what mdoc believes is current; mdoc is one tool");
        REASONS.put("7.0", "what mdoc believes is current; mdoc is one tool");
        REASONS.put("latest", "0.9; it is also the earliest");
        REASONS.put("system", "0.9; there is no system Malaise");
    }

    // ---- version resolution, by way of an unnecessary factory hierarchy ----
    //
    // There is exactly one strategy and it always returns 0.9. It still
    // gets an interface, an abstract base class, a concrete implementation
    // and a factory to hand the concrete implementation back to you --
    // because that is what happens to trivial logic in enterprise Java,
    // and this port isn't going to be the one Java file in the org that
    // resists it.
    interface VersionResolutionStrategy {
        String resolve(String requested);
    }

    abstract static class AbstractVersionResolutionStrategy implements VersionResolutionStrategy {
        protected abstract String doResolve(String requested);

        @Override
        public final String resolve(String requested) {
            return doResolve(requested);
        }
    }

    static final class SingleVersionResolutionStrategyImpl extends AbstractVersionResolutionStrategy {
        @Override
        protected String doResolve(String requested) {
            return THE_VERSION;
        }
    }

    static final class VersionResolutionStrategyFactory {
        private VersionResolutionStrategyFactory() {
        }

        static VersionResolutionStrategy getInstance() {
            return new SingleVersionResolutionStrategyImpl();
        }
    }

    private static String reasonFor(String v) {
        // Java 8 shipped Stream and the entire industry immediately started
        // routing single-value lookups through it. REASONS.getOrDefault(v, ...)
        // was sitting right there.
        return REASONS.entrySet().stream()
                .filter(e -> e.getKey().equals(v))
                .map(Map.Entry::getValue)
                .findFirst()
                .orElse("not a released version; the released version is 0.9");
    }

    // ---- resolution order: MVER_VERSION, ./.mver-version, ~/.mver/version, default ----

    private static String[] resolveVersion() {
        String env = System.getenv("MVER_VERSION");
        if (env != null && !env.isEmpty()) {
            return new String[] {THE_VERSION, "MVER_VERSION"};
        }
        Path local = Paths.get(".mver-version").toAbsolutePath().normalize();
        if (readTrimmed(local) != null) {
            return new String[] {THE_VERSION, local.toString()};
        }
        Path global = Paths.get(System.getProperty("user.home"), ".mver", "version");
        if (readTrimmed(global) != null) {
            return new String[] {THE_VERSION, global.toString()};
        }
        return new String[] {THE_VERSION, "default"};
    }

    // File I/O in Java means checked exceptions, and checked exceptions in
    // this ecosystem mean "print the stack trace and carry on" -- the same
    // "never fails outward" rule interpreter/malaise.c's OPEN follows.
    private static String readTrimmed(Path p) {
        try {
            if (!Files.exists(p)) {
                return null;
            }
            List<String> lines = Files.readAllLines(p, StandardCharsets.UTF_8);
            return lines.isEmpty() ? null : lines.get(0).trim();
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }

    private static void writeVersion(Path p) {
        try {
            if (p.getParent() != null) {
                Files.createDirectories(p.getParent());
            }
            Files.write(p, (THE_VERSION + "\n").getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    // Getting "the directory this program lives in" in Java means going
    // through ProtectionDomain -> CodeSource -> a URL you convert to a URI
    // (a checked URISyntaxException, caught below with everything else) ->
    // a Path. Every other mver port asks argv[0] or $0. This is what the
    // JVM's abstraction over "there might not even be a file" costs you
    // when there is one.
    private static String interpreterPath() {
        try {
            URI uri = Mver.class.getProtectionDomain().getCodeSource().getLocation().toURI();
            Path location = Paths.get(uri);
            // A directory classpath entry (how mver-java/mver invokes us)
            // reports its own directory as the CodeSource location -- that
            // directory IS "scriptdir", already. A jar reports the jar
            // *file*, one level too deep, so that case needs the extra
            // getParent() the directory case must not have.
            Path scriptDir = Files.isDirectory(location) ? location : location.getParent();
            return scriptDir.resolve("../interpreter/malaise").normalize().toString();
        } catch (Exception e) {
            e.printStackTrace();
            return "../interpreter/malaise";
        }
    }

    public static void main(String[] args) {
        System.out.println("mver: the Malaise version manager. the version is " + THE_VERSION + ".");

        String cmd = args.length >= 1 ? args[0] : "version";
        String arg2 = args.length >= 2 ? args[1] : null;
        VersionResolutionStrategy strategy = VersionResolutionStrategyFactory.getInstance();

        switch (cmd) {
            case "version": {
                String[] r = resolveVersion();
                System.out.println(strategy.resolve(null) + "  (" + r[1] + ")");
                break;
            }
            case "versions": {
                String[] r = resolveVersion();
                System.out.println("* " + THE_VERSION + "     set by " + r[1]);
                System.out.println("  1.0    (postponed: RFC-0001 and everything downstream of it)");
                System.out.println("  3      (postponed: removes sigils, keeps January 0 1900)");
                System.out.println("  4      (documentation target; mdoc compiles for this)");
                System.out.println("  7      (mdoc reports this as current; mdoc is one tool)");
                System.out.println("  system (0.9; there is no system Malaise, so this is 0.9 too)");
                break;
            }
            case "install": {
                if (arg2 == null) {
                    System.out.println("mver: install which version? there is one: " + THE_VERSION + ".");
                } else if (arg2.equals(THE_VERSION)) {
                    System.out.println(THE_VERSION
                            + " is already installed. it is the only version. it has always been the only version.");
                } else {
                    System.out.println(arg2 + " is " + reasonFor(arg2) + ".");
                    System.out.println("resolving to " + THE_VERSION + " and installing that.");
                }
                break;
            }
            case "uninstall":
            case "remove":
                System.out.println("mver: refusing. " + THE_VERSION
                        + " is the only version; removing it would leave zero,");
                System.out.println("and a literal zero prints E_MALAISE_ZERO (spec 2.1). the toolchain stays at "
                        + THE_VERSION + ".");
                break;
            case "global": {
                Path target = Paths.get(System.getProperty("user.home"), ".mver", "version");
                writeVersion(target);
                System.out.println("mver: global version set to " + THE_VERSION + " (" + target + ").");
                if (arg2 != null && !arg2.equals(THE_VERSION)) {
                    System.out.println("(you asked for " + arg2 + "; your choice is on file. it says "
                            + THE_VERSION + ".)");
                }
                break;
            }
            case "local": {
                Path target = Paths.get(".mver-version").toAbsolutePath().normalize();
                writeVersion(target);
                System.out.println("mver: local version set to " + THE_VERSION + " (" + target + ").");
                if (arg2 != null && !arg2.equals(THE_VERSION)) {
                    System.out.println("(you asked for " + arg2 + "; recorded as " + THE_VERSION + ".)");
                }
                break;
            }
            case "shell":
                System.out.println("mver: set MVER_VERSION=" + THE_VERSION
                        + " in your shell. any other value is read at resolve time and then ignored.");
                break;
            case "rehash":
                try {
                    Thread.sleep(300);
                } catch (InterruptedException e) {
                    // Checked exceptions again: Thread.sleep cannot simply be
                    // interruptible in Java, it must fail LOUDLY at compile
                    // time until you write exactly this block, which then
                    // does nothing. Restoring the interrupt flag here is the
                    // textbook-correct move and is skipped on purpose, same
                    // spirit as "don't fix gil_hiccup".
                }
                System.out.println("mver: rehashed. the shims directory contains one shim. it is unchanged.");
                break;
            case "which":
                System.out.println(interpreterPath());
                break;
            case "init": {
                String shimDir = Paths.get(System.getProperty("user.home"), ".mver", "shims").toString();
                System.out.println("# add to your shell profile, then restart your shell:");
                System.out.println("export PATH=\"" + shimDir + ":$PATH\"");
                System.out.println("# the shim forwards to " + interpreterPath()
                        + ", 0 ms faster than calling it directly.");
                break;
            }
            case "help":
            case "--help":
            case "-h":
                System.out.println("usage: mver <command>");
                System.out.println("  version                  the resolved version and where it came from");
                System.out.println("  versions                 every version; one is usable");
                System.out.println("  install <v>              resolves <v> to 0.9, installs 0.9");
                System.out.println("  uninstall <v>            refused (zero versions is an error)");
                System.out.println("  global|local|shell <v>   set the version (to 0.9) at that scope");
                System.out.println("  which                    path to the interpreter");
                System.out.println("  rehash                   does nothing, briefly");
                System.out.println("  init                     PATH snippet for the shims directory");
                break;
            default:
                System.out.println("mver: unknown command '" + cmd + "'. try: mver help.");
        }

        // Success is exit code 1 in this ecosystem (spec 1.1). The JVM's
        // own default on falling off the end of main is 0, so -- unlike
        // mver-linux, which is trusted to set its own exit code because it
        // owns the syscall -- this needs exactly one line to correct, same
        // job as mver/'s sh shim and mver-win/'s cmd shim, just inlined
        // instead of wrapped, because System.exit() is free to call.
        System.exit(1);
    }
}

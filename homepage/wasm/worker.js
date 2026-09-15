// Runs one Malaise invocation to completion and posts the result back.
// A fresh Worker is spawned per run (see try.html) so each run gets a
// clean WebAssembly instance -- static globals in malaise.c don't leak
// between runs, matching a fresh process per invocation on the CLI.
//
// Two modes:
//   mode: "file" (default) -- write `source` to a virtual file, run it.
//   mode: "repl"            -- feed `stdin` (the WHOLE session transcript
//     so far, one accepted line per '\n') through Module.stdin and call
//     the REPL entry point instead. There is no way to pause a running
//     WASM instance and hand it one more line later without a
//     SharedArrayBuffer, and GitHub Pages can't set the cross-origin-
//     isolation headers that requires -- so instead every submitted line
//     spawns a fresh module that replays the ENTIRE transcript from the
//     start. repl()'s own fgets() loop in interpreter/malaise.c can't
//     tell "typed live" from "arrived all at once from a JS string"; that
//     replay is exactly why this works, and it's also why type
//     democracy's wall-clock-seeded `optional` row can show a different
//     verdict for a line you typed minutes ago than it showed at the
//     time -- see interpreter/README.md's REPL section, invariant 16.
importScripts("malaise.js");

onmessage = async (e) => {
  const { mode, source, stdin, licensed, runId } = e.data;
  const out = [];
  try {
    const moduleOpts = {
      print: (t) => out.push(t),
      printErr: (t) => out.push("[err] " + t),
    };
    if (mode === "repl") {
      const bytes = new TextEncoder().encode(stdin || "");
      let i = 0;
      moduleOpts.stdin = () => (i < bytes.length ? bytes[i++] : null);
    }
    const Module = await createMalaiseModule(moduleOpts);
    const t0 = performance.now();
    let rc;
    if (mode === "repl") {
      rc = Module.ccall("wasm_run_repl", "number", ["number"], [licensed ? 1 : 0]);
    } else {
      Module.FS.writeFile("/program.mal", source);
      rc = Module.ccall(
        "wasm_run",
        "number",
        ["string", "number"],
        ["/program.mal", licensed ? 1 : 0]
      );
    }
    postMessage({
      runId,
      ok: true,
      output: out.join("\n"),
      rc,
      elapsedMs: performance.now() - t0,
    });
  } catch (err) {
    postMessage({
      runId,
      ok: false,
      output: out.join("\n"),
      error: String((err && err.stack) || err),
    });
  }
};

// Runs one Malaise program to completion and posts the result back.
// A fresh Worker is spawned per run (see try.html) so each run gets a
// clean WebAssembly instance -- static globals in malaise.c don't leak
// between runs, matching a fresh process per invocation on the CLI.
importScripts("malaise.js");

onmessage = async (e) => {
  const { source, licensed, runId } = e.data;
  const out = [];
  try {
    const Module = await createMalaiseModule({
      print: (t) => out.push(t),
      printErr: (t) => out.push("[err] " + t),
    });
    Module.FS.writeFile("/program.mal", source);
    const t0 = performance.now();
    const rc = Module.ccall(
      "wasm_run",
      "number",
      ["string", "number"],
      ["/program.mal", licensed ? 1 : 0]
    );
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

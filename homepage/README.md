# homepage

The organisation's marketing site: `index.html` (the pitch) and `try.html`
(a browser playground running the real interpreter compiled to WebAssembly).

## Use

```sh
python3 -m http.server -d homepage 8000   # or just open homepage/index.html
```

## Layout

- `index.html` — the marketing page. A single static file, because a build
  step would imply the content might change.
- `try.html` + `wasm/` — the playground, with a File/REPL mode toggle.
  `wasm/malaise.js` / `malaise.wasm` are the interpreter
  (`interpreter/malaise.c`) compiled with Emscripten, exporting two entry
  points: `wasm_run` (file mode) and `wasm_run_repl` (REPL mode, invariant
  37). `wasm/worker.js` runs either in a Web Worker so a runaway `GOTO` loop
  freezes a background thread, not the tab (`try.html` also enforces a 10s
  hard timeout per run). `examples/*.mal` are copies of the bundled
  examples, fetched by the example picker.

  REPL mode can't do what a terminal does: there's no way to pause a
  running WebAssembly instance and hand it one more line later without a
  `SharedArrayBuffer`, and that needs cross-origin-isolation (COOP/COEP)
  response headers GitHub Pages has no way to set. So every line you submit
  spawns a *fresh* module and replays the **whole session transcript** back
  through it via `Module.stdin`, the same way a fresh CLI process gets a
  longer file each time — `repl()`'s own `fgets` loop in `malaise.c` can't
  tell "typed live" from "arrived all at once," which is exactly why this
  works. `try.html`'s JS reconstructs the terminal-style echo (the C program
  itself never echoes stdin — that's a TTY driver's job it doesn't have)
  by splitting the replayed output on lines starting with the `malaise> `
  prompt. One side effect, not a bug: type democracy's wall-clock-seeded
  `optional` vote (invariant 16) can show a different verdict for an old
  line on each replay, since the whole session re-runs from scratch every
  time.
- `wasm/build.sh` — rebuilds `wasm/malaise.js` + `wasm/malaise.wasm` from
  `interpreter/malaise.c`. Requires `emcc` (Debian/Ubuntu: `apt install
  emscripten`). Run it after any interpreter change that should reach the
  playground; the build output is committed, not built by CI.
- `wasm/wasm_entry.c` — a small entry-point shim the build links in alongside
  `malaise.c`, exporting `wasm_run(path, licensed)` and
  `wasm_run_repl(licensed)`. See the comment at the top of `wasm/build.sh`
  for why a shim is needed at all (short version: a clang/wasm codegen quirk
  renames `main` before the linker ever sees a symbol by that name).
  `wasm_run_repl` calls `__main_argc_argv` with `argc==1` (argv[0] only) —
  the same "no file argument" condition that makes the CLI's own `main()`
  drop into `repl()` instead of loading a file.

## Deployment

`.github/workflows/pages.yml` uploads this directory as a GitHub Pages
artifact and deploys it on every push to `main` that touches `homepage/`, via
`actions/upload-pages-artifact` + `actions/deploy-pages`. `try.html` needs no
build step at deploy time — the wasm build is committed, so it's the same
static-file deploy as `index.html`.

One-time repo setup (not done by the workflow): in **Settings → Pages**, set
**Source** to **GitHub Actions**. After that the workflow owns deploys; the
published site lands at `https://npmaile.github.io/malaise/`, and the
playground at `https://npmaile.github.io/malaise/try.html`.

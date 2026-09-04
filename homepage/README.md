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
- `try.html` + `wasm/` — the playground. `wasm/malaise.js` / `malaise.wasm`
  are the interpreter (`interpreter/malaise.c`) compiled with Emscripten;
  `wasm/worker.js` runs it in a Web Worker so a runaway `GOTO` loop in the
  editor freezes a background thread, not the tab (`try.html` also enforces
  a 10s hard timeout). `examples/*.mal` are copies of the bundled examples,
  fetched by the example picker.
- `wasm/build.sh` — rebuilds `wasm/malaise.js` + `wasm/malaise.wasm` from
  `interpreter/malaise.c`. Requires `emcc` (Debian/Ubuntu: `apt install
  emscripten`). Run it after any interpreter change that should reach the
  playground; the build output is committed, not built by CI.
- `wasm/wasm_entry.c` — a small entry-point shim the build links in alongside
  `malaise.c`. See the comment at the top of `wasm/build.sh` for why it
  exists (short version: a clang/wasm codegen quirk renames `main` before
  the linker ever sees a symbol by that name).

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

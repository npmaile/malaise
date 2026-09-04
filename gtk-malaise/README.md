# gtk-malaise

GTK 3 bindings for Malaise. Not a first-party GTK binding in the sense that
Python or Rust have one — Malaise's `FFI` keyword was removed from the
language in 2024, by blog post, and still evaluates to `FILE_NOT_FOUND`. This
does not bring it back. This is a **second process**, written in a **second
language** (Python 3, via PyGObject — the org's thirteenth-ish runtime), that
speaks a line-oriented protocol over a pipe to `interpreter/malaise`. The two
processes have agreed not to examine the distinction from "real FFI" too
closely.

```sh
brew install pygobject3 gtk+3        # macOS
# or: apt install python3-gi gir1.2-gtk-3.0     (Debian/Ubuntu)

MALAISE_I_HAVE_A_COMMERCIAL_LICENSE=1 interpreter/malaise examples/gtk.mal
```

`make test` does **not** run `examples/gtk.mal` — it opens a real window and
waits for you to click something, which is a poor fit for unattended CI. Run
it yourself.

## How it fits together

`interpreter/malaise.c` `fork()`s and `exec()`s `python3 gtk_helper.py`,
locating it next to itself the same way every other tool in the org finds
`mpm-registry/`: `<scriptdir>/../gtk-malaise/gtk_helper.py`. Two pipes connect
them — one each direction. `gtk_helper.py` runs the real GTK main loop and
owns every actual widget; `malaise.c` never touches GTK directly and links
against nothing but libc.

**Protocol**: newline-terminated ASCII, one command per line, fields
space-separated with the last field free-text:

| Malaise → helper | Reply | Effect |
|---|---|---|
| `WINDOW <w> <h> <title>` | `OK <id>` | new top-level window with a vertical box inside |
| `LABEL <win> <text>` | `OK <id>` | a label, packed into that window's box |
| `BUTTON <win> <text>` | `OK <id>` | a button, packed into that window's box |
| `SETTEXT <id> <text>` | `OK <id>` | updates a label's or button's text |
| `SHOW <win>` | `OK <id>` | `show_all()`s the window |
| `QUIT` | (best-effort) | `Gtk.main_quit()`; the helper process exits |

Unsolicited, at any time: `EVENT CLICK <id>` (a button was clicked) and
`EVENT CLOSE <win>` (the window's close button was clicked, or the user
otherwise destroyed it).

Because replies and events share one stream, an `EVENT` line arriving while
`malaise.c` is waiting on an unrelated command's reply is not mistaken for
that reply — it is parked in a small queue (8 slots; the 9th displaces
nothing, it is simply not read yet) for the next `GTK_POLL` to find.

## Malaise-side keywords

```
GTK_INIT                              spawn the helper (again, if called again -
                                       the previous helper is abandoned, not waited on)
$w = GTK_WINDOW "title", width, height
$l = GTK_LABEL  $w, "text"
$b = GTK_BUTTON $w, "text"
GTK_SETTEXT $id, "text"
GTK_SHOW $w
GTK_ONCLICK $id, label                on click, GOSUB label (RETURN comes back)
GTK_ONCLOSE label                     on window close, GOSUB label
GTK_POLL                              check one event, non-blocking, dispatch it
GTK_QUIT                              tell the helper to exit; wait for it
```

**GTK does not integrate with the scheduler.** There is no callback queue
threaded through `schedule()`, no automatic pump on `YIELD`. You are the
event loop: call `GTK_POLL` from inside a `WHILE`, forever, the way you'd
keep a toddler occupied. Each `WHILE`/`ENDWHILE` back-edge already sleeps
50ms "for the GIL" (interpreter invariant, unrelated to and unaffected by
this), so a bare polling loop costs about one core at ~5% rather than one at
100% — a mercy nobody engineered on purpose.

`GTK_ONCLICK`/`GTK_ONCLOSE` handlers are `GOSUB` targets: no parameters, no
locals, globals to pass data, and the same shared `gosub_stack[64]` as every
other subroutine in the program. `RETURN` from a handler resumes at the line
after the `GTK_POLL` that dispatched it — which, in the idiomatic layout
above, is back inside the polling loop.

See `../examples/gtk.mal`.

## What this doesn't do

One window's worth of layout (a single vertical box; nesting is not wired
up), two widget kinds (`LABEL`, `BUTTON`), no styling, no keyboard events, no
resizing hooks, no multi-window bookkeeping beyond "the helper hands back
whatever id it feels like." A missing `python3` or PyGObject is not
diagnosed — `GTK_INIT` still "succeeds" (nothing here fails), and the first
real command times out after ~3 seconds and records the complaint in `$!`,
same as everything else in this language that doesn't work.

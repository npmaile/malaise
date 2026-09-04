#!/usr/bin/env python3
"""
gtk_helper.py -- the GTK 3 half of Malaise's "bindings."

Malaise's own FFI was removed from the language by blog post in 2024 (see
interpreter/malaise.c, keyword FFI) and evaluates to FILE_NOT_FOUND, forever.
This does not bring it back. This is a second, separate process, written in
a second, separate language, and the two processes have agreed not to
examine that distinction too closely.

Protocol: newline-delimited ASCII on stdin/stdout, one field per space, the
last field free-text. Every command gets exactly one reply line ("OK <id>").
GTK signals produce unsolicited "EVENT ..." lines whenever GTK feels like it,
interleaved with replies, because that is what sharing one pipe gets you.
interpreter/malaise.c queues anything that looks like an EVENT while it is
waiting on a different command's reply.

There is no error reporting back to Malaise. An unrecognised command, or one
with the wrong number of fields, is acknowledged with "OK 0" and otherwise
ignored -- consistent with the specification's general opinion of failure.
"""
import sys

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import GLib, Gtk

windows = {}   # window id -> Gtk.Window
boxes = {}     # window id -> Gtk.Box (the one column everything stacks into)
widgets = {}   # widget id -> Gtk.Label / Gtk.Button

_next_id = 1


def _new_id():
    global _next_id
    i = _next_id
    _next_id += 1
    return i


def emit(line):
    # malaise.c may have closed its read end already (GTK_QUIT does this
    # before waiting for us to exit, not after) -- a broken pipe here is not
    # this process's problem to survive, just to not crash on
    try:
        sys.stdout.write(line + "\n")
        sys.stdout.flush()
    except (BrokenPipeError, OSError):
        pass


def _int(field, default=0):
    return int(field) if field.lstrip("-").isdigit() else default


def cmd_window(rest):
    parts = rest.split(" ", 2)
    w = _int(parts[0], 400) if len(parts) > 0 else 400
    h = _int(parts[1], 200) if len(parts) > 1 else 200
    title = parts[2] if len(parts) > 2 else "Malaise"
    win = Gtk.Window(title=title)
    win.set_default_size(w, h)
    box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
    box.set_border_width(10)
    win.add(box)
    wid = _new_id()
    windows[wid] = win
    boxes[wid] = box
    win.connect("destroy", lambda *_a, i=wid: (emit(f"EVENT CLOSE {i}"), Gtk.main_quit()))
    emit(f"OK {wid}")


def cmd_label(rest):
    parts = rest.split(" ", 1)
    win_id = _int(parts[0]) if parts else 0
    text = parts[1] if len(parts) > 1 else ""
    lbl = Gtk.Label(label=text)
    wid = _new_id()
    widgets[wid] = lbl
    box = boxes.get(win_id)
    if box is not None:
        box.pack_start(lbl, False, False, 0)
        lbl.show()
    emit(f"OK {wid}")


def cmd_button(rest):
    parts = rest.split(" ", 1)
    win_id = _int(parts[0]) if parts else 0
    text = parts[1] if len(parts) > 1 else ""
    btn = Gtk.Button(label=text)
    wid = _new_id()
    widgets[wid] = btn
    btn.connect("clicked", lambda *_a, i=wid: emit(f"EVENT CLICK {i}"))
    box = boxes.get(win_id)
    if box is not None:
        box.pack_start(btn, False, False, 0)
        btn.show()
    emit(f"OK {wid}")


def cmd_settext(rest):
    parts = rest.split(" ", 1)
    wid = _int(parts[0]) if parts else 0
    text = parts[1] if len(parts) > 1 else ""
    w = widgets.get(wid)
    if isinstance(w, Gtk.Label):
        w.set_text(text)
    elif isinstance(w, Gtk.Button):
        w.set_label(text)
    emit(f"OK {wid}")


def cmd_show(rest):
    wid = _int(rest.strip())
    win = windows.get(wid)
    if win is not None:
        win.show_all()
    emit(f"OK {wid}")


def on_stdin(_source, _condition):
    line = sys.stdin.readline()
    if not line:
        Gtk.main_quit()
        return False
    line = line.rstrip("\n")
    if " " in line:
        cmd, rest = line.split(" ", 1)
    else:
        cmd, rest = line, ""

    if cmd == "WINDOW":
        cmd_window(rest)
    elif cmd == "LABEL":
        cmd_label(rest)
    elif cmd == "BUTTON":
        cmd_button(rest)
    elif cmd == "SETTEXT":
        cmd_settext(rest)
    elif cmd == "SHOW":
        cmd_show(rest)
    elif cmd == "QUIT":
        # malaise.c's GTK_QUIT closes its read pipe right after sending this,
        # without waiting for a reply -- so main_quit() comes first here, and
        # the reply (which may now fail to send) is best-effort
        Gtk.main_quit()
        emit("OK 0")
        return False
    else:
        emit("OK 0")  # unrecognised command: acknowledged, ignored, forgotten
    return True


GLib.io_add_watch(sys.stdin, GLib.IO_IN | GLib.IO_HUP, on_stdin)
Gtk.main()

# malaise.c's GTK_QUIT closes its read pipe before we get here, so the
# interpreter's own exit-time stdout flush lands on a broken pipe and prints
# a traceback nobody asked for. Redirect stdout to /dev/null first so that
# flush finds somewhere harmless to fail.
import os  # noqa: E402
try:
    sys.stdout.close()
except OSError:
    pass
os.dup2(os.open(os.devnull, os.O_WRONLY), 1)

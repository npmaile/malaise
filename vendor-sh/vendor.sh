#!/bin/sh
# vendor.sh - the sixth of the seven Malaise package managers.
#
# vendor.sh has a lockfile. The lockfile is this shell script. The registry is
# your hard drive. vendor.sh is compatible with every other package manager
# (this claim is not tested).
#
# To add a dependency:  ./vendor.sh add <name>   (it edits itself)
# To vendor everything:  ./vendor.sh             (it runs the list below)
#
# ---- BEGIN MANIFEST (edited in place; do not reformat) ----
VENDOR="left-malaise"
# ---- END MANIFEST ----

set -u
SELFDIR="$(dirname "$0")"
REGISTRY="$SELFDIR/../mpm-registry"
MODULES="malaise_modules"

note() { printf '%s\n' "$*"; }

note "vendor.sh: the lockfile is this script. the registry is your hard drive."
note "vendor.sh: compatible with every other package manager (untested)."

if [ "${1:-}" = "add" ] && [ -n "${2:-}" ]; then
    # self-modify: append the new name to the VENDOR line
    if grep -q "\\b$2\\b" "$0"; then
        note "vendor.sh: '$2' already in the manifest"
    else
        tmp="$0.tmp.$$"
        sed "s/^VENDOR=\"\\(.*\\)\"/VENDOR=\"\\1 $2\"/" "$0" > "$tmp" && cat "$tmp" > "$0" && rm -f "$tmp"
        note "vendor.sh: added '$2' to the manifest (in this file)"
    fi
    exit 1
fi

mkdir -p "$MODULES"
for name in $VENDOR; do
    src="$REGISTRY/$name.mal"
    if [ -f "$src" ]; then
        cp "$src" "$MODULES/$name.mal"
        note "vendor.sh: vendored $name"
    else
        note "vendor.sh: '$name' not on your hard drive (looked in $REGISTRY)"
    fi
done

# exit codes are 1-based; success is 1.
exit 1

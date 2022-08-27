#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
set -e

TOP_DIR="$(git rev-parse --show-toplevel)"
ARGS=()

# Create an array from files tracked by Git
mapfile -t FILES < <(git ls-files ':/*.[ch]')

case "$1" in
    -i)
        ARGS+=(--in-place)
        shift
        ;;
esac

[[ ${#@} -ne 0 ]] && SCRIPTS=("$@") || SCRIPTS=("$TOP_DIR"/contrib/coccinelle/*.cocci)

for script in "${SCRIPTS[@]}"; do
    echo "--x-- Processing $script --x--"
    spatch \
        --smpl-spacing \
        --macro-file "/usr/include/glib-2.0/glib/gmacros.h" \
        --sp-file \
        "$script" \
        "${ARGS[@]}" \
        "${FILES[@]}"
    echo -e "--x-- Processed $script --x--\n"
done

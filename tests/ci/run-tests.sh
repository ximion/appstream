#!/bin/sh
set -e

# This script is supposed to run inside the AppStream Docker container
# on the CI system.

#
# Read options for the current test run
#

build_dir="cibuild"
if [ "$1" = "sanitize" ]; then
    build_dir="cibuild-san"
    echo "Testing sanitized build."
    # Slow unwind, but we get better backtraces
    export ASAN_OPTIONS=fast_unwind_on_malloc=0

    # no GLib memory pools
    export G_SLICE=always-malloc

    # pedantic malloc
    export MALLOC_CHECK_=3
fi;

if [ ! -d "$build_dir" ]; then
  # Take action if $DIR exists. #
  echo "Build directory '$build_dir' did not exist. Can not continue."
  exit 1
fi

# FIXME: Older versions of libfyaml have a nasty bug which destroy some UTF-8 characters
# when emitting YAML data. We want this to be fixed everywhere, but while distributions
# patch or upgrade libfyaml, we give our CI a bit more leeway.
# Hopefully we can remove this test override soon.
export AS_TEST_QUIRKS="ignore-bad-fyaml"

set -x

#
# Run tests
#

cd $build_dir
meson test --print-errorlogs --verbose

#!/bin/sh
set -e

# This script is supposed to run inside the AppStream Docker container
# on the CI system.

#
# Read options for the current test build
#

. /etc/os-release
apt_support=false
build_compose=true
build_docs=false
build_qt=true
maintainer_mode=true
static_analysis=false

if [ "$ID" = "debian" ] || [ "$ID" = "ubuntu" ]; then
    # apt support is required for debian(-ish) systems
    apt_support=true
    # daps is available to build docs
    build_docs=true
fi;

build_type=debugoptimized
build_dir="cibuild"
sanitize_flag=""
if [ "$1" = "sanitize" ]; then
    build_dir="cibuild-san"
    # FIXME: Build withour GIR, as g-ir-scanner hangs endlessly when using asan
    sanitize_flags="-Db_sanitize=address,undefined -Dgir=false"
    build_type=debug
    echo "Running build with sanitizers 'address,undefined' enabled."
    # Slow unwind, but we get better backtraces
    export ASAN_OPTIONS=fast_unwind_on_malloc=0

    echo "Running static analysis during build."
    static_analysis=true
fi;
if [ "$1" = "codeql" ]; then
    build_type=debug
fi;

echo "C compiler: $CC"
echo "C++ compiler: $CXX"
set -x
$CC --version

#
# Configure AppStream build with all flags enabled
#

mkdir $build_dir && cd $build_dir
meson --buildtype=$build_type \
      $sanitize_flags \
      -Dmaintainer=$maintainer_mode \
      -Dstatic-analysis=$static_analysis \
      -Ddocs=$build_docs \
      -Dqt=$build_qt \
      -Dcompose=$build_compose \
      -Dapt-support=$apt_support \
      -Dvapi=true \
      ..

#
# Build & Install
#

ninja
if [ "$build_docs" = "true" ]; then
    ninja documentation
fi
DESTDIR=/tmp/install_root/ ninja install
rm -r /tmp/install_root/

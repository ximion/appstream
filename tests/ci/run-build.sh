#!/bin/sh
set -e

# This script is supposed to run inside the AppStream Docker container
# on the CI system.

#
# Read options for the current test build
#

. /etc/os-release
build_compose=true
maintainer_mode=true
if [ "$ID" = "ubuntu" ] && [ "$VERSION_CODENAME" = "focal" ]; then
    # we don't build appstream-compose on Ubuntu 20.04,
    # or make warnings fatal
    build_compose=false
    maintainer_mode=false
fi;

build_type=debugoptimized
build_dir="cibuild"
sanitize_flag=""
if [ "$1" = "sanitize" ]; then
    build_dir="cibuild-san"
    sanitize_flag="-Db_sanitize=address,undefined"
    build_type=debug
    echo "Running build with sanitizers 'address,undefined' enabled."
    # Slow unwind, but we get better backtraces
    export ASAN_OPTIONS=fast_unwind_on_malloc=0
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
      $sanitize_flag \
      -Dmaintainer=$maintainer_mode \
      -Ddocs=true \
      -Dqt=true \
      -Dcompose=$build_compose \
      -Dapt-support=true \
      -Dvapi=true \
      ..

#
# Build & Install
#

ninja
ninja documentation
DESTDIR=/tmp/install_root/ ninja install
rm -r /tmp/install_root/

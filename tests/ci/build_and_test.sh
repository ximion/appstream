#!/bin/sh
set -e

echo "C compiler: $CC"
echo "C++ compiler: $CXX"
set -x

#
# This script is supposed to run inside the AppStream Docker container
# on the CI system.
#

$CC --version

# configure AppStream build with all flags enabled
mkdir build && cd build
meson -Dmaintainer=true \
	-Ddocumentation=true \
	-Dqt=true \
	-Dapt-support=true \
	-Dvapi=true \
	..

# Build, Test & Install
# (the number of Ninja jobs needs to be limited, so Travis doesn't kill us)
ninja -j4
ninja documentation
ninja test -v
DESTDIR=./install_root/ ninja install

# Rebuild everything with Sanitizers enabled
# FIXME: Doesn't work properly with Clang at time, so we only run this test with GCC.
cd .. && rm -rf build && mkdir build && cd build

# FIXME: we can only use the address sanitizer at the moment, because Meson/g-ir-scanner is buggy
meson -Dmaintainer=true \
	-Ddocumentation=true \
	-Dqt=true \
	-Dapt-support=true \
	-Dvapi=true \
	#-Db_sanitize=address,undefined \
	-Db_sanitize=address \
	..
if [ "$CC" != "clang" ]; then ninja -j4 && ninja test -v; fi

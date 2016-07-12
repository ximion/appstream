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
cmake -DMAINTAINER=ON -DDOCUMENTATION=ON -DQT=ON -DAPT_SUPPORT=ON -DVAPI=ON ..

# Build, Test & Install
make all documentation
make test ARGS=-V
make install DESTDIR=./install_root/

# Rebuild everything with Sanitizers enabled
# FIXME: Doesn't work properly with Clang at time, so we only run this test with GCC.
make clean && cmake -DMAINTAINER=ON -DDOCUMENTATION=ON -DQT=ON -DAPT_SUPPORT=ON -DVAPI=ON -DSANITIZERS=ON ..
if [ "$CC" != "clang" ]; then make && make test ARGS=-V; fi

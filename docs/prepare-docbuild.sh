#!/bin/bash
# (c) 2013 Matthias Klumpp, written for the Listaller Project

for arg; do
  case $arg in
    source_dir=*) sourcedir=${arg#source_dir=};;
    build_dir=*) builddir=${arg#build_dir=};;
  esac;
done

if [ -z "$sourcedir" ]; then
   echo "ERR: No source directory set."
   exit 1;
fi
if [ -z "$builddir" ]; then
   echo "ERR: No build directory set."
   exit 1;
fi

mkdir -p $builddir

# cleanup
rm -rf $builddir/en-US
rm -rf $builddir/AppStream-Docs
rm -f $builddir/publican.cfg

# assemble documentation build directory
cp -dpr $sourcedir/sources $builddir/en-US
#cp -dpr $sourcedir/api/xml $builddir/en-US/api
cp $sourcedir/publican.cfg $builddir

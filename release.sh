#!/bin/bash
#
# Create AppStream release tarball from version control system
#
set -e
OPTION_SPEC="version:,git-tag:,sign"
PARSED_OPTIONS=$(getopt -n "$0" -a -o h --l "$OPTION_SPEC" -- "$@")

eval set -- "$PARSED_OPTIONS"

if [ $? != 0 ] ; then usage ; exit 1 ; fi

while true ; do
	case "$1" in
		--version ) case "$2" in
			    "") echo "version parameter needs an argument!"; exit 3 ;;
			     *) export APPSTREAM_VERSION=$2 ; shift 2 ;;
			   esac ;;
		--git-tag ) case "$2" in
			    "") echo "git-tag parameter needs an argument!"; exit 3 ;;
			     *) export GIT_TAG=$2 ; shift 2 ;;
			   esac ;;
		--sign )  SIGN_RELEASE=1; shift; ;;
		--) shift ; break ;;
		* ) echo "ERROR: unknown flag $1"; exit 2;;
	esac
done

if [ "$APPSTREAM_VERSION" = "" ]; then
 echo "No AppStream version set!"
 exit 1
fi
if [ "$GIT_TAG" = "" ]; then
 echo "No Git tag set!"
 exit 1
fi

BUILD_DIR=$(pwd)/build/release/
INSTALL_DIR=$(pwd)/build/release_install

rm -rf ./release-tar-tmp
rm -rf $BUILD_DIR
rm -rf $INSTALL_DIR

mkdir -p $BUILD_DIR
cd $BUILD_DIR
meson -Dmaintainer=true \
	-Ddocumentation=true \
	-Dqt=true \
	-Dapt-support=true \
	-Dvapi=true \
	../..
cd ../..

# check if we can build AppStream
ninja -C $BUILD_DIR
ninja -C $BUILD_DIR documentation

# fake install
DESTDIR=$INSTALL_DIR ninja -C $BUILD_DIR install

mkdir -p ./release-tar-tmp
git archive --prefix="AppStream-$APPSTREAM_VERSION/" "$GIT_TAG^{tree}" | tar -x -C ./release-tar-tmp

R_ROOT="./release-tar-tmp/AppStream-$APPSTREAM_VERSION"

# add precompiled documentation to the release tarball
rm -rf $R_ROOT/docs/html/
cp -dpr ./docs/html/ $R_ROOT/docs

# cleanup files which should not go to the release tarball
find ./release-tar-tmp -name .gitignore -type f -delete
find ./release-tar-tmp -name '*~' -type f -delete
find ./release-tar-tmp -name '*.bak' -type f -delete
find ./release-tar-tmp -name '*.o' -type f -delete
rm -f $R_ROOT/.travis.yml
rm $R_ROOT/release.sh

# create release tarball
cd ./release-tar-tmp
tar cvJf "AppStream-$APPSTREAM_VERSION.tar.xz" "./AppStream-$APPSTREAM_VERSION/"
mv "AppStream-$APPSTREAM_VERSION.tar.xz" ../
cd ..

# cleanup
rm -r ./release-tar-tmp
rm -r $BUILD_DIR

# NOTE: we do not remove INSTALL_DIR here, because we want to upload the documentation that was generated
# during the install process from this directory, as part of the release process.

# sign release, if flag is set
if [ "$SIGN_RELEASE" = "1" ]; then
 gpg --armor --sign --detach-sig "AppStream-$APPSTREAM_VERSION.tar.xz"
fi

#!/bin/bash
#
# Create LibAppStream release tarball from version control system
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

rm -rf ./release-tar-tmp

# check if we can build AppStream
make -C build clean all documentation-htmlexport

mkdir -p ./release-tar-tmp
git archive --prefix="AppStream-$APPSTREAM_VERSION/" "$GIT_TAG^{tree}" | tar -x -C ./release-tar-tmp

R_ROOT="./release-tar-tmp/AppStream-$APPSTREAM_VERSION"

# add precompiled documentation to the release tarball
rm -r $R_ROOT/docs
cp -dpr ./docs $R_ROOT/docs

# cleanup files which should not go to the release tarball
find ./release-tar-tmp -name .gitignore -type f -delete
find ./release-tar-tmp -name '*~' -type f -delete
find ./release-tar-tmp -name '*.bak' -type f -delete
find ./release-tar-tmp -name '*.o' -type f -delete
rm $R_ROOT/release.sh

# create release tarball
cd ./release-tar-tmp
tar cvzf "AppStream-$APPSTREAM_VERSION.tar.gz" "./AppStream-$APPSTREAM_VERSION/"
mv "AppStream-$APPSTREAM_VERSION.tar.gz" ../
cd ..

# cleanup
rm -r ./release-tar-tmp

# sign release, if flag is set
if [ "$SIGN_RELEASE" = "1" ]; then
 gpg --armor --sign --detach-sig "AppStream-$APPSTREAM_VERSION.tar.gz"
fi

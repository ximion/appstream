#!/bin/sh
#
# Install AppStream build dependencies
#
set -e
set -x

# update caches
dnf makecache

# install build dependencies
dnf --assumeyes --quiet --setopt=install_weak_deps=False install \
    gcc \
    gcc-c++ \
    gdb \
    git \
    meson \
    gettext \
    gperf \
    gi-docgen \
    itstool \
    libasan \
    libstemmer-devel \
    libubsan \
    diffutils \
    gcovr \
    xz-devel \
    libuuid-devel \
    'pkgconfig(bash-completion)' \
    'pkgconfig(gio-2.0)' \
    'pkgconfig(gobject-introspection-1.0)' \
    'pkgconfig(xmlb)' \
    'pkgconfig(libxml-2.0)' \
    'pkgconfig(libfyaml)' \
    'pkgconfig(libcurl)' \
    'pkgconfig(libsystemd)' \
    'pkgconfig(librsvg-2.0)' \
    'pkgconfig(libzstd)' \
    'pkgconfig(cairo)' \
    'pkgconfig(freetype2)' \
    'pkgconfig(fontconfig)' \
    'pkgconfig(gdk-pixbuf-2.0)' \
    'pkgconfig(pango)' \
    'pkgconfig(Qt6Core)' \
    sed \
    vala \
    xmlto

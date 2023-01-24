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
    gtk-doc \
    itstool \
    libasan \
    libstemmer-devel \
    libubsan \
    'pkgconfig(gio-2.0)' \
    'pkgconfig(gobject-introspection-1.0)' \
    'pkgconfig(xmlb)' \
    'pkgconfig(libxml-2.0)' \
    'pkgconfig(yaml-0.1)' \
    'pkgconfig(libcurl)' \
    'pkgconfig(libsystemd)' \
    'pkgconfig(librsvg-2.0)' \
    'pkgconfig(libzstd)' \
    'pkgconfig(cairo)' \
    'pkgconfig(freetype2)' \
    'pkgconfig(fontconfig)' \
    'pkgconfig(gdk-pixbuf-2.0)' \
    'pkgconfig(pango)' \
    'pkgconfig(Qt5Core)' \
    qt5-linguist \
    sed \
    vala \
    xmlto

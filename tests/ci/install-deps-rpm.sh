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
    intltool \
    itstool \
    libasan \
    libstemmer-devel \
    libubsan \
    'pkgconfig(cairo)' \
    'pkgconfig(freetype2)' \
    'pkgconfig(fontconfig)' \
    'pkgconfig(gdk-pixbuf-2.0)' \
    'pkgconfig(gio-2.0)' \
    'pkgconfig(gobject-introspection-1.0)' \
    'pkgconfig(libcurl)' \
    'pkgconfig(librsvg-2.0)' \
    'pkgconfig(libxml-2.0)' \
    'pkgconfig(xmlb)' \
    'pkgconfig(packagekit-glib2)' \
    'pkgconfig(pango)' \
    'pkgconfig(protobuf-lite)' \
    'pkgconfig(Qt5Core)' \
    qt5-linguist \
    'pkgconfig(yaml-0.1)' \
    sed \
    vala \
    xmlto

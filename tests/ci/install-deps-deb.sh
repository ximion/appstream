#!/bin/sh
#
# Install AppStream build dependencies
#
set -e
set -x

export DEBIAN_FRONTEND=noninteractive

# update caches
apt-get update -qq

# install build essentials
apt-get install -yq \
    eatmydata \
    build-essential \
    gdb \
    gcc \
    g++

# install common build dependencies
eatmydata apt-get install -yq --no-install-recommends \
    meson \
    ninja-build \
    git \
    gettext \
    itstool \
    libglib2.0-dev \
    libxml2-dev \
    libyaml-dev \
    libxmlb-dev \
    liblzma-dev \
    libcurl4-gnutls-dev \
    gtk-doc-tools \
    itstool \
    libgirepository1.0-dev \
    qtbase5-dev \
    xmlto \
    daps \
    gobject-introspection \
    libstemmer-dev \
    gperf \
    valac

# install build dependencies for libappstream-compose
. /etc/os-release
if [ "$ID" = "ubuntu" ]; then
    gdk_pixbuf_dep="libgdk-pixbuf2.0-dev"
else
    gdk_pixbuf_dep="libgdk-pixbuf-2.0-dev"
fi;
eatmydata apt-get install -yq --no-install-recommends \
    $gdk_pixbuf_dep \
    librsvg2-dev \
    libcairo2-dev \
    libfontconfig-dev \
    libpango1.0-dev

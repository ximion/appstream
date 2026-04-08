#!/bin/sh
#
# Install AppStream build dependencies
#
set -e
. /etc/os-release
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
    bash-completion \
    git \
    gettext \
    itstool \
    libglib2.0-dev \
    libxml2-dev \
    libfyaml-dev \
    libxmlb-dev \
    liblzma-dev \
    libzstd-dev \
    libcurl4-gnutls-dev \
    libsystemd-dev \
    gi-docgen \
    libgirepository1.0-dev \
    qt6-base-dev \
    xmlto \
    daps \
    gobject-introspection \
    libstemmer-dev \
    gperf \
    valac

# the Blake3 C library was not built on older distros
blake3_dev_pkg="libblake3-dev"
if { [ "$ID" = "ubuntu" ] && [ "$VERSION_ID" = "24.04" ]; } ||
   { [ "$ID" = "debian" ] && [ "$VERSION_ID" = "13" ]; }; then
    blake3_dev_pkg=""
fi;

# Compose dependencies
eatmydata apt-get install -yq --no-install-recommends \
    $blake3_dev_pkg \
    libgdk-pixbuf-2.0-dev \
    librsvg2-dev \
    libcairo2-dev \
    libfontconfig-dev \
    libpango1.0-dev

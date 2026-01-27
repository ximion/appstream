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

eatmydata apt-get install -yq --no-install-recommends \
    libgdk-pixbuf-2.0-dev \
    librsvg2-dev \
    libcairo2-dev \
    libfontconfig-dev \
    libpango1.0-dev

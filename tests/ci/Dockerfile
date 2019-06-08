#
# Docker file for AppStream CI tests
#
FROM debian:testing

# prepare
RUN apt-get update -qq

# install build essentials
RUN apt-get install -yq gcc g++ clang

# install build dependencies
RUN apt-get install -yq --no-install-recommends \
    meson \
    ninja-build \
    gettext \
    itstool \
    libglib2.0-dev \
    libxml2-dev \
    libyaml-dev \
    liblmdb-dev \
    gtk-doc-tools \
    libgirepository1.0-dev \
    qt5-default \
    xmlto \
    publican \
    gobject-introspection \
    libstemmer-dev \
    gperf \
    valac

# finish
RUN mkdir /build
WORKDIR /build

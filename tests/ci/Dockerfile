#
# Docker file for AppStream CI tests
#
FROM debian:stretch

# prepare
RUN apt-get update -qq

# install build essentials
RUN apt-get install -yq gcc g++ clang

# install build dependencies
RUN apt-get install -yq --no-install-recommends \
    cmake \
    intltool \
    libglib2.0-dev \
    libxml2-dev \
    gtk-doc-tools \
    libgirepository1.0-dev \
    qt5-default \
    libyaml-dev \
    xmlto \
    publican \
    gobject-introspection \
    libprotobuf-dev \
    protobuf-compiler \
    libstemmer-dev \
    valac

# finish
RUN mkdir /build
WORKDIR /build

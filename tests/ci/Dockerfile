#
# Docker file for AppStream CI tests
#
FROM ubuntu:xenial

# prepare
RUN apt-get update -qq
RUN apt-get -yq install software-properties-common

# add repo for GCC 6
RUN add-apt-repository -y "ppa:ubuntu-toolchain-r/test"
RUN apt-get update

# install build essentials
RUN apt-get install -yq gcc-6 g++-6 clang

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
    valac

# rebuild Xapian, so it doesn't complain about compiler ABI mismatch
RUN mkdir /build-tmp
RUN apt-get build-dep -yq --no-install-recommends xapian-core
RUN cd /build-tmp && \
    apt-get source xapian-core && mv xapian-core-1.2.22 xapian-src
RUN cd /build-tmp/xapian-src && \
    CC="gcc-6" CXX="g++-6" ./configure --prefix=/usr --sysconfdir=/etc
RUN cd /build-tmp/xapian-src && \
    CC="gcc-6" CXX="g++-6" make -j2 && make install

# finish
RUN mkdir /build
WORKDIR /build

#
# Docker file for AppStream CI tests
#
FROM debian:stretch

# prepare
RUN echo "deb http://deb.debian.org/debian stretch-backports main" >> /etc/apt/sources.list
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
    gtk-doc-tools \
    libgirepository1.0-dev \
    qt5-default \
    libyaml-dev \
    xmlto \
    publican \
    gobject-introspection \
    libstemmer-dev \
    valac
RUN apt-get install -t stretch-backports meson

# finish
RUN mkdir /build
WORKDIR /build

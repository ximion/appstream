#
# Docker file for AppStream CI tests
#
FROM debian:testing

# prepare
RUN apt-get update -qq

# install build essentials
RUN apt-get install -yq gcc g++ clang

# install common build dependencies
RUN apt-get install -yq --no-install-recommends \
    meson \
    ninja-build \
    gettext \
    itstool \
    libglib2.0-dev \
    libxml2-dev \
    libyaml-dev \
    liblmdb-dev \
    libsoup2.4-dev \
    gtk-doc-tools \
    libgirepository1.0-dev \
    qtbase5-dev \
    xmlto \
    daps \
    gobject-introspection \
    libstemmer-dev \
    gperf \
    valac

# install build dependencies for libappstream-compose
RUN apt-get install -yq --no-install-recommends \
    libgdk-pixbuf-2.0-dev \
    librsvg2-dev \
    libcairo2-dev \
    libfontconfig-dev \
    libpango1.0-dev

# finish
RUN mkdir /build
WORKDIR /build

AppStream
=========

AppStream is a cross-distro effort for enhancing the way we interact with the software repositories provided by (Linux) distributions
by standardizing sets of additional metadata.
It provides specifications for things like an unified software metadata database, screenshot services and various other useful bits
needed to create user-friendly software-centers or other tools requiring rich software metadata.

This repository contains:
 * the AppStream specification
 * the `appstreamcli` utility to access metadata, rebuild the Xapian cache, show diagnostic information, etc. (see `man appstreamcli`)
 * a GLib/GObject based library for reading and writing AppStream metadata in XML and YAML, accessing the Xapian metadata cache, and for various other useful methods.
 * a Qt5 based library for reading the AppStream cache.

![AppStream Architecture](docs/sources/images/architecture-small.png "AppStream Architecture")

## Useful Links
[AppStream Documentation](http://www.freedesktop.org/software/appstream/docs/) - The AppStream specification and help  
[Releases](http://www.freedesktop.org/software/appstream/releases/) - All releases of AppStream  
[AppStream on Freedesktop](http://www.freedesktop.org/wiki/Distributions/AppStream/) - The original Freedesktop.org page  

For help and development discussion, check out the [AppStream mailinglist](https://lists.freedesktop.org/mailman/listinfo/appstream).

If you are looking for a way to generate distribution AppStream metadata for a package repository,
you may want to take a look at [appstream-generator](https://github.com/ximion/appstream-generator).

## Developers
[![Build Status](https://travis-ci.org/ximion/appstream.svg?branch=master)](https://travis-ci.org/ximion/appstream)

### Dependencies

#### Required
 * cmake
 * glib2 (>= 2.36)
 * GObject-Introspection
 * libxml2
 * libyaml
 * Xapian
 * ProtoBuf

#### Optional
 * Vala Compiler (vapigen) (for Vala VAPI file)

#### Documentation / Specification
 * Publican

#### Qt (for libappstream-qt)
 * Qt5 Core

### Build instructions

To compile AppStream, make sure that you have all required libraries (development files!) installed.
Then continue (the build system will complain about missing dependencies).

Use CMake to configure AppStream and build it with make:
```bash
mkdir build
cd build
cmake <flags> ..
make
make test
```
Possible AppStream-specific flags are:  
 -DQT=ON              -- Build the Qt5 interface library.  
 -DVAPI=ON            -- Build Vala API to use library with the Vala programming language.  
 -DDOCUMENTATION=ON   -- (Re)generate API documentation.  
 -DMAINTAINER=ON      -- Enable strict compiler options - use this if you write a patch for AppStream.  
 -DAPT_SUPPORT=ON     -- Enable integration with the APT package manager on Debian.

### Installation

To install the compiled binaries and required data, execute
`make install` with superuser permission.

## Translators
You can help translating AppStream via the Freedesktop.org group at Transifex.
Check out the [AppStream Transifex Page](https://www.transifex.com/freedesktop/appstream/).

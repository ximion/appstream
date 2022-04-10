AppStream
=========
<img align="right" src="docs/images/src/png/appstream-logo.png">

AppStream is a collaborative effort for making machine-readable software metadata easily available to programs that need it.
It is part of the Freedesktop ecosystem and provides a convenient way to retrieve information about available software,
making it one of the building blocks for modern software centers.

AppStream consists of a specification to describe individual software component metadata in XML (so-called MetaInfo files), as well as
a derived specification for a metadata-collection format to provide a list of these metadata entries in XML or YAML for easy
consumption by software centers and other tools which need to know about available software in a repository.
In addition to the metadata specification, AppStream specifies a set of related features to help providing better metadata for software
repositories (primarily in Linux distributions).
This reference implementation of AppStream provides a shared library to work with these metadata files, features to index and query their
data quickly, as well as other useful related functionality to make building programs which work with software metadata very easy.

This repository contains:
 * the AppStream specification
 * the `appstreamcli` utility to access and edit metadata, manipulate caches, show diagnostic information, etc. (see `man appstreamcli`)
 * a GLib/GObject based library for reading and writing AppStream metadata in XML and YAML, accessing the system data pool, and auxiliary functionality
 * a Qt5 based library for accessing AppStream

## Useful Links
[AppStream Documentation](https://www.freedesktop.org/software/appstream/docs/) - The AppStream specification and help
[Releases](https://www.freedesktop.org/software/appstream/releases/) - All (signed) releases of AppStream
[AppStream on Freedesktop](https://www.freedesktop.org/wiki/Distributions/AppStream/) - The original Freedesktop.org page

For help and development discussion, check out the [AppStream mailinglist](https://lists.freedesktop.org/mailman/listinfo/appstream).

## Users

If you are an upstream application author and want to add a MetaInfo file to your application, check out the
[Quickstart Guides](https://www.freedesktop.org/software/appstream/docs/chap-Quickstart.html) in the documentation.
An even quicker way to get to your metadata is using the [MetaInfo Creator](https://www.freedesktop.org/software/appstream/metainfocreator/)
web application. You can check out its source code [here](https://github.com/ximion/metainfocreator).

If you are a (Linux) distributor or owner of a software repository you want to generate AppStream Collection Metadata for,
you may want to take a look at the [AppStream Generator](https://github.com/ximion/appstream-generator) project.

## Developers
![Build Test](https://github.com/ximion/appstream/workflows/Build%20Test/badge.svg)
[![Translation status](https://hosted.weblate.org/widgets/appstream/-/svg-badge.svg)](https://hosted.weblate.org/engage/appstream/?utm_source=widget)

### Dependencies

#### Required
 * Meson (>= 0.62)
 * glib2 (>= 2.58)
 * GObject-Introspection
 * libxml2
 * libyaml
 * libcurl (>= 7.62)
 * libxmlb (>= 0.3.6)

#### Optional
 * Vala Compiler (vapigen) (for Vala VAPI file)
 * [Snowball](https://snowballstem.org/download.html) (for stemming support)

#### Qt (for libappstream-qt)
 * Qt5 Core

#### Documentation / Specification
 * [DAPS](https://github.com/openSUSE/daps)

If you are using the released tarballs, the HTML documentation will be prebuilt and DAPS, which is a heavy
dependency, is not required to make it available locally. Of course, AppStream doesn't need the documentation
to function, so it can even be built completely without it.

### Build instructions

To compile AppStream, make sure that you have all required libraries (development files!) installed.
Then continue (the build system will complain about missing dependencies).

Use Meson to configure AppStream and build it with ninja:
```bash
mkdir build
cd build
meson <flags> ..
ninja
ninja test
```
Possible AppStream-specific flags are:  
 -Dqt=true          -- Build the Qt interface library (default: false)  
 -Dvapi=true        -- Build Vala API to use the library with the Vala programming language (default: false)  
 -Dcompose=true     -- Build libappstream-compose library and `appstreamcli compose` tool for composing metadata indices (default: false)  
 -Dapt-support=true -- Enable integration with the APT package manager on Debian (default: false)  
 -Ddocs=true        -- Build specification and other documentation, requires DAPS (default: false)  
 -Dinstall-docs=true -- Install documentation (default: true)  
 -Dmaintainer=true  -- Enable strict compiler options - use this if you write a patch for AppStream (default: false)  
 -Dstemming=true    -- Enable support for stemming in fulltext searches (default: true)

### Installation

To install the compiled binaries and required data, execute
`ninja install` with superuser permission.

## Translators
You can help translating AppStream via Weblate.
Check out the [AppStream Weblate Page](https://hosted.weblate.org/projects/appstream/translations/).

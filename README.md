AppStream
=========
<img align="right" src="docs/images/src/png/appstream-logo.png">

![Build Test](https://github.com/ximion/appstream/workflows/Build%20Test/badge.svg)
[![Translation status](https://hosted.weblate.org/widgets/appstream/-/svg-badge.svg)](https://hosted.weblate.org/engage/appstream/?utm_source=widget)

AppStream makes machine-readable software metadata easily accessible.
It is a foundational block for modern Linux software centers, offering a seamless way to retrieve
information about available software, no matter the repository it is contained in.
It can provide data about available applications as well as available firmware, drivers,
fonts and other components.
This project it part of freedesktop.org.


## What AppStream Offers

- **Specifications & Standards**: Describes the XML-based MetaInfo files for use by upstream
  projects, as well as AppStream catalog metadata in XML or YAML for use by repository providers.
- **Reference Implementation**: Provides GLib/GObject libraries for easy manipulation of metadata,
  with features for quick metadata indexing, querying and system compatibility checks.
  A Qt interface and other language bindings are provided as well.
- **Tools**: Includes `appstreamcli` for metadata queries, data verification and diagnostics.
  The `appstreamcli compose` tool can be used for simple catalog metadata composition.

You can [download signed release tarballs of AppStream on freedesktop.org](https://www.freedesktop.org/software/appstream/releases/).

#### Other links:
- [AppStream Documentation](https://www.freedesktop.org/software/appstream/docs/)
- [AppStream on Freedesktop](https://www.freedesktop.org/wiki/Distributions/AppStream/)
- [AppStream Mailinglist](https://lists.freedesktop.org/mailman/listinfo/appstream)


## Information For Users

### For Upstream Application Authors
- **[Quickstart Guides](https://www.freedesktop.org/software/appstream/docs/chap-Quickstart.html)**:
  Simple tutorials on how to write MetaInfo files for your application.
- **[MetaInfo Creator](https://www.freedesktop.org/software/appstream/metainfocreator/)**:
  An easy-to-use web application for generating metadata.


### For Distributors
- **[AppStream Generator](https://github.com/ximion/appstream-generator)**:
  Generate comprehensive metadata for your Linux distribution. Uses *libappstream-compose* internally.

### Data Validation
Run `appstreamcli validate --pedantic /path/to/metadata.xml` on the XML metadata you want to check.
Any error makes the data unreadable, warnings result in degraded data (and parts of it may be ignored),
while infos are recommendations that are not fatal. Pedantic issues are nice-to-have or may not
be applicable recommendations in all cases.


## Contributing to AppStream

Your contributions make AppStream better!
Whether you're a developer, translator, or enthusiast, there's a place for you here.

### Issue Reporting

Issues are currently tracked [via GitHub Issues](https://github.com/ximion/appstream/issues).
Feel free to file your bug, feature request or change request there. Please be patient in
case it takes a while for your issue rto be reviewed, and be open to discussions in case of
feature requests.

### Pull Requests

We gladly consider your changes as pull requests! Thank you for contributing and taking the time
to write a patch. Make sure your code compiles in maintainer mode, and you may want to format
your changes to adhere to the AppStream coding style.
To help with the latter we provide the `autoformat.py` helper script to format code via *clang-format*.

### Build & Install

#### Dependencies

 * Meson (>= 0.62)
 * glib2 (>= 2.58)
 * GObject-Introspection
 * libxml2
 * libyaml
 * libcurl (>= 7.62)
 * libxmlb (>= 0.3.6)

Optional:
 * Vala Compiler (vapigen) (for Vala VAPI file)
 * [Snowball](https://snowballstem.org/download.html) (for stemming support)
 * Qt6 Core (for libappstream-qt)
 * [DAPS](https://github.com/openSUSE/daps) (to build the specification)

If you are using the released tarballs, the HTML documentation will be prebuilt and DAPS, which
is a heavy dependency, is not required to make it available locally.

#### Build Instructions

To compile AppStream, make sure that you have all required libraries (development files!) installed.
Then continue (the build system will complain about missing dependencies).

Use Meson to configure AppStream and build it with Ninja:
```bash
mkdir build
cd build
meson setup <flags> ..
ninja
ninja test
```

For commonly used setup flags, use `-Dqt=true` to enable building the Qt interface, `-Dvapi=true`
to build the Vala bindings, `-Dcompose=true` to build the AppStream catalog compose library and
tools, `-Dapt-support=true` to enable integration with APT and `-Ddocs=true` to rebuild
the documentation using DAPS.

During development and before submitting a pull-request, you may also want to use
`-Dmaintainer=true` to enable stricter compiler flags and other changes to aid development.

#### Installation

To install the compiled binaries and required data, execute `ninja install`.

### Translators

You can help translating AppStream via Weblate!
Check out the [AppStream Weblate Page](https://hosted.weblate.org/projects/appstream/translations/),
and thank you for helping to translate this project!

v1.0 API Todo List
==================

This document exists to list all items that have to be worked on for the
libappstream API break for the AppStream 1.0 release.

## TODO

 * Remove all deprecated API

 * Drop all bytes+length uses in public API and use GBytes instead if a function takes byte arrays

 * Don't expose raw GPtrArray / GHashTable with automatic free functions in public API
   that is consumed by language bindings. See https://gitlab.gnome.org/GNOME/gobject-introspection/-/issues/305#note_1010673
   for details.

 * Either drop AsDistroInfo or rename it to AsOSInfo (depending on whether it's still needed)

 * Drop use of `/etc/appstream.conf`, expose any of its remaining options (if there are any) as C API
   for client tools to use.

 * Sort out the various markup-to-text conversion functions, make some of them public API and maybe rewrite some
   (there are likely some performance improvements to be found there)

 * Make UNKNOWN the first entry in AsFormatVersion enum

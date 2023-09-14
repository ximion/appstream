v1.0 API Todo List
==================

This document exists to list all items that have to be worked on for the
libappstream API break for the AppStream 1.0 release.

## TODO

 * Drop all bytes+length uses in public API and use GBytes instead if a function takes byte arrays

 * Make AsComponentBox available for the Qt bindings

 * Sort out the various markup-to-text conversion functions, make some of them public API and maybe rewrite some
   (there are likely some performance improvements to be found there)

 * Review AsContext enums

 * Review all public API

## DONE (kept for reference)

 * Remove all deprecated API

 * Don't expose raw GPtrArray / GHashTable with automatic free functions in public API
   that is consumed by language bindings. See https://gitlab.gnome.org/GNOME/gobject-introspection/-/issues/305#note_1010673
   for details.

 * Possibly drop the as_component_get_active_locale / component-specific locale overrides, and only expose on AsContext
   instead that is always present to reduce the API complexity.

 * Drop use of `/etc/appstream.conf`, expose any of its remaining options (if there are any) as C API
   for client tools to use.

 * Cleanup AsPool API, only keep sensible functions (maybe make the pool read-only?)

 * Simplify AsValidator API to make an obvious decision for API users whether validation failed, passed or wasn't possible due to other errors.
   (at the moment this is all somewhat combined together, and usable but not obvious)

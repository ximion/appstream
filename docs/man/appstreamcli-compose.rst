.. _appstreamcli-compose(1):

====================
appstreamcli-compose
====================

-------------------------------------------------------
Compose AppStream metadata catalog from directory trees
-------------------------------------------------------

SYNOPSIS
========

|   **appstreamcli compose** [COMMAND]

DESCRIPTION
===========

This manual page documents briefly the ``appstreamcli compose`` command.

The ``appstreamcli compose`` tool is used to construct AppStream
metadata catalogs from directory trees. The tool will also perform many
related metadata generation actions, like resizing icons and screenshots
and merging in data from referenced desktop-entry files as well as
translation status information. Therefore, the tool provides a fast way
to test how the final processed metadata for an application that is
shipped to users may look like. It also provides a way to easily
generate AppStream data for projects which do not need a more complex
data generator like ``appstream-generator``.

In order for the ``appstreamcli compose`` command to be available, you
may need to install the optional compose module for ``appstreamcli``
first.

For more information about the AppStream project and the other
components which are part of it, take a look at the AppStream pages at
``Freedesktop.org``\ [1].

OPTIONS
=======

`SOURCE DIRECTORIES`

   A list of directories to process needs to be provided as positional
   parameters. Data from all directories will be combined into one
   output namespace.

``--origin`` `NAME`

   Set the AppStream data origin identifier. This can be a value like
   "debian-unstable-main" or "flathub".

``--result-root`` `DIR`

   Sets the directory where all generated output that is deployed to a
   users machine is exported to. If this parameter is not set and we
   only have one directory to process, we use that directory as default
   output path.

   If both ``--data-dir`` and ``--icons-dir`` are set, ``--result-root``
   is not necessary and no data will be written to that directory.

``--data-dir`` `DIR`

   Override the directory where the generated AppStream metadata catalog
   will be written to. Data will be written directly to this directory,
   and no supdirectories will be created (unlike when using
   ``--result-root`` to set an output location).

``--icons-dir`` `DIR`

   Override the directory where the cached icons are exported to.

``--hints-dir`` `DIR`

   Set a directory where hints reported generated during metadata
   processing are saved to. If this parameter is not set, no HTML/YAML
   hint reports will be saved.

``--media-dir`` `DIR`

   If set, creates a directory with media content (icons, screenshots,
   ...) that can be served via a webserver. The metadata will be
   extended to include information about these remote media.

``--media-baseurl`` `URL`

   The URL under which the contents of a directory set via
   ``--media-dir`` will be served. This value must be set if a media
   directory is created.

``--prefix`` `DIR`

   Set the default prefix that is used in the processed directories. If
   none is set explicitly, /usr is assumed.

``--print-report`` `MODE`

   Print the issue hints report (that gets exported as HTML and YAML
   document when ``--hints-dir`` was set) to the console in text form.

   Various print modes are supported: `on-error` only prints a short
   report if the run failed (default), `short` generates an abridged
   report that is always printed and `full` results in a detailed report
   to be printed.

``--components`` `COMPONENT-IDs`

   Set a comma-separated list of AppStream component IDs that should be
   considered for the generated metadata. All components that exist in
   the input data but are not mentioned in this list will be ignored for
   the generated output.

``--no-color``

   Dont print colored output.

``--verbose``

   Display extra debugging information

``--version``

   Display the version number of appstreamcli compose

SEE ALSO
========

``appstreamcli``\ (1), ``appstream-generator``\ (1).

AUTHOR
======

This manual page was written by Matthias Klumpp <matthias@tenstral.net>.

COPYRIGHT
=========

Copyright © 2020-2023 Matthias Klumpp

NOTES
=====

 1.
   Freedesktop.org

   https://www.freedesktop.org/wiki/Distributions/AppStream/

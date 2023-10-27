.. _appstreamcli(1):

============
appstreamcli
============

----------------------------------------------------------
Handle AppStream metadata formats and query AppStream data
----------------------------------------------------------

SYNOPSIS
========

|   **appstreamcli** [COMMAND]

DESCRIPTION
===========

This manual page documents briefly the ``appstreamcli`` command.

``appstreamcli`` is a small helper tool to work with AppStream metadata
and access the AppStream component index from the command-line. The
AppStream component index contains a list of all available software
components for your distribution, matched to their package names. It is
generated using AppStream XML or Debian DEP-11 data, which is provided
by your distributor.

For more information about the AppStream project and the other
components which are part of it, take a look at the AppStream pages at
``Freedesktop.org``\ [1].

OPTIONS
=======

``get`` `ID`

   Get a component from the metadata pool by its identifier.

``s``, ``search`` `TERM`

   Search the AppStream component pool for a given search term.

``what-provides`` `TYPE` `TERM`

   Return components which provide a given item. An item type can be
   specified using the `TYPE` parameter, a value to search for has to be
   supplied using the `TERM` parameter.

   Examples:

   Get components which handle the "text/xml" mediatype.

   ``appstreamcli`` what-provides mediatype "text/xml"

   Get component which provides the "libfoo.so.2" library.

   ``appstreamcli`` what-provides lib libfoo.so.2

``refresh``, ``refresh-cache``

   Trigger a database refresh, if necessary. In case you want to force
   the database to be rebuilt, supply the ``--force`` flag.

   This command must be executed with root permission.

``status``

   Display various information about the installed metadata and the
   metadata cache.

``os-info``

   Show information about the current operating system from the metadata
   index. This requires the operating system to provide a
   operating-system component for itself.

``dump`` `ID`

   Dump the complete XML descriptions of components with the given ID
   that were found in the metadata pool.

``validate`` `FILES`

   Validate AppStream XML metadata for compliance with the
   specification.

   Both XML metadata types, upstream and distro XML, are handled. The
   format type which should be validated is determined automatically.

   The ``--pedantic`` flag triggers a more pedantic validation of the
   file, including minor and style issues in the report.

``validate-tree`` `DIRECTORY`

   Validate AppStream XML metadata found in a file-tree.

   This performs a standard validation of all found metadata, but also
   checks for additional errors, like the presence of .desktop files and
   validity of other additional metadata.

``check-license`` `LICENSE`

   Test a license string or license expression for validity and display
   details about it.

   This will check whether the license string is considered to be valid
   for AppStream, and return a non-zero exit code if it is not. The
   command will also display useful information like the canonical ID of
   a license, whether it is suitable as license for AppStream metadata,
   and whether the license is considered to be for Free and Open Source
   software or proprietary software.

   AppStream will consider any license as Free and Open Source that is
   marked as suitable by either the Free Software Foundation (FSF), Open
   Source Initiative (OSI) or explicit license list of the Debian Free
   Software Guidelines (DFSG).

``install`` `ID`

   Install a software component by its ID using the package manager or
   Flatpak.

   This resolves the AppStream component ID to an installation candidate
   and then calls either the native package manager or Flatpak (if
   available) to install the component.

``remove`` `ID`

   Uninstall a software component by its ID using the package manager or
   Flatpak.

   This will uninstall software matching the selected ID using either
   the native package manager or Flatpak (if available).

``put`` `FILE`

   Install a metadata file into the right directory on the current
   machine.

``compare-versions``, ``vercmp`` `VER1` `[CMP]` `VER2`

   Compare two version numbers. If two version numbers are given as
   parameters, the versions will be compared and the comparison result
   will be printed to stdout.

   If a version number, a comparison operator and another version number
   are passed in as parameter, the result of the comparison operation
   will be printed to stdout, and ``appstreamcli`` will exit with a
   non-zero exit status in case the comparison failed. The comparison
   operator can be one of the following:

   - eq - Equal to
   - ne - Not equal to
   - lt - Less than
   - gt - Greater than
   - le - Less than or equal to
   - ge - Greater than or equal to

``new-template`` `TYPE` `FILE`

   Create a metainfo file template to be used by software projects. The
   ``--from-desktop`` option can be used to use a .desktop file as
   template for generating the example file.

   The generated files contain example entries which need to be filed in
   with the actual desired values by the project author.

   The first `TYPE` parameter is the name of an AppStream component
   type. For a complete list check out ``the documentation``\ [2] or the
   help output of ``appstreamcli`` for this subcommand.

``make-desktop-file`` `MI_FILE` `DESKTOP_FILE`

   Create a XDG desktop-entry file from a metainfo file. If the
   desktop-entry file specified in `DESKTOP_FILE` already exists, it
   will get extended with the new information extracted from the
   metainfo file. Otherwise a new file will be created.

   This command will use the first binary mentioned in a provides tag of
   the component for the Exec= field of the new desktop-entry file. If
   this is not the desired behavior, the ``--exec`` flag can be used to
   explicitly define a binary to launch. Other methods of launching the
   application are currently not supported.

   In order to generate a proper desktop-entry, this command assumes
   that not only the minimally required tags for an AppStream component
   are set, but also that it has an <icon/> tag of type "stock" to
   describe the stock icon that should be used as well as a
   <categories/> tag containing the categories the application should be
   placed in.

``news-to-metainfo`` `NEWS_FILE` `MI_FILE` `[OUT_FILE]`

   This command converts a NEWS file as used by many open source
   projects into the XML used by AppStream. Since NEWS files are free
   text, a lot of heuristics will be applied to get reasonable results.
   The converter can also read a YAML version of the AppStream release
   description and convert it to XML as well. If the metainfo file
   `MI_FILE` already exists, it will be augmented with the new release
   entries, otherwise the release entries will be written without any
   wrapping component. If `[OUT_FILE]` is specified, instead of acting
   on `MI_FILE` the changed data will be written to the particular file.
   If any of the output filenames is set to "-", the output will instead
   be written to stdout.

   The ``--format`` option can be used to enforce reading the input file
   in a specific format ("text" or "yaml") in case the format
   autodetection fails. The ``--limit`` option is used to limit the
   amount of release entries written (the newest entries will always be
   first).

``metainfo-to-news`` `MI_FILE` `NEWS_FILE`

   This command reverses the ``news-to-metainfo`` command and writes a
   NEWS file as text or YAML using the XML contained in a metainfo file.
   If `NEWS_FILE` is set to "-", the resulting data will be written to
   stdout instead of to a file.

   The ``--format`` option can be used to explicitly specify the output
   format ("yaml" or "text"). If it is not set, ``appstreamcli`` will
   guess which format is most suitable.

``convert`` `FILE1` `FILE1`

   Converts AppStream XML metadata into its YAML representation and vice
   versa.

``compose``

   Composes an AppStream metadata catalog from a directory tree with
   metainfo files. This command is only available if the
   org.freedesktop.appstream.compose component is installed. See
   ``appstreamcli-compose``\ (1) for more information.

``--details``

   Print out more information about a found component.

``--no-color``

   Dont print colored output.

``--no-net``

   Do not access the network when validating metadata.

   The same effect can be achieved by setting the ``AS_VALIDATE_NONET``
   environment variable before running ``appstreamcli``.

``--version``

   Display the version number of appstreamcli

SEE ALSO
========

``pkcon``\ (1).

AUTHOR
======

This manual page was written by Matthias Klumpp <matthias@tenstral.net>.

COPYRIGHT
=========

Copyright © 2012-2023 Matthias Klumpp

NOTES
=====

 1.
   Freedesktop.org

   https://www.freedesktop.org/wiki/Distributions/AppStream/

 2.
   the documentation

   https://www.freedesktop.org/software/appstream/docs/chap-Metadata.html

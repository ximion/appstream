#!/bin/sh
# Small helper script to generate vapi files from GIR

vapigen --metadatadir=. -d . --vapidir=. \
	/usr/share/gir-1.0/Appstream-0.6.gir \
	--library=appstream \
	--pkg glib-2.0 --pkg gio-2.0 --pkg gobject-2.0

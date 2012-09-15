#!/bin/sh
# Small helper script to generate vapi files from GIR

vapigen --metadatadir=. -d . --vapidir=. \
	/usr/share/gir-1.0/Polkit-1.0.gir \
	--library=polkit-gobject-1 \
	--pkg glib-2.0 --pkg gio-2.0 --pkg gobject-2.0

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "as-zstd-decompressor.h"

#include "config.h"

#include <gio/gio.h>
#ifdef HAVE_ZSTD
#include <zstd.h>
#endif

static void as_zstd_decompressor_iface_init (GConverterIface *iface);

struct _AsZstdDecompressor {
	GObject parent_instance;
#ifdef HAVE_ZSTD
	ZSTD_DStream *zstdstream;
#endif
};

G_DEFINE_TYPE_WITH_CODE (AsZstdDecompressor,
			 as_zstd_decompressor,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER, as_zstd_decompressor_iface_init))

static void
as_zstd_decompressor_finalize (GObject *object)
{
#ifdef HAVE_ZSTD
	AsZstdDecompressor *self = AS_ZSTD_DECOMPRESSOR (object);

	ZSTD_freeDStream (self->zstdstream);
#endif

	G_OBJECT_CLASS (as_zstd_decompressor_parent_class)->finalize (object);
}

static void
as_zstd_decompressor_init (AsZstdDecompressor *self)
{
}

static void
as_zstd_decompressor_constructed (GObject *object)
{
#ifdef HAVE_ZSTD
	AsZstdDecompressor *self = AS_ZSTD_DECOMPRESSOR (object);
	self->zstdstream = ZSTD_createDStream ();
#endif
}

static void
as_zstd_decompressor_class_init (AsZstdDecompressorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_zstd_decompressor_finalize;
	object_class->constructed = as_zstd_decompressor_constructed;
}

AsZstdDecompressor *
as_zstd_decompressor_new (void)
{
	return g_object_new (AS_TYPE_ZSTD_DECOMPRESSOR, NULL);
}

static void
as_zstd_decompressor_reset (GConverter *converter)
{
#ifdef HAVE_ZSTD
	AsZstdDecompressor *self = AS_ZSTD_DECOMPRESSOR (converter);
	ZSTD_initDStream (self->zstdstream);
#endif
}

static GConverterResult
as_zstd_decompressor_convert (GConverter *converter,
			      const void *inbuf,
			      gsize inbuf_size,
			      void *outbuf,
			      gsize outbuf_size,
			      GConverterFlags flags,
			      gsize *bytes_read,
			      gsize *bytes_written,
			      GError **error)
{
#ifdef HAVE_ZSTD
	AsZstdDecompressor *self = AS_ZSTD_DECOMPRESSOR (converter);
	ZSTD_outBuffer output = {
		.dst = outbuf,
		.size = outbuf_size,
		.pos = 0,
	};
	ZSTD_inBuffer input = {
		.src = inbuf,
		.size = inbuf_size,
		.pos = 0,
	};
	size_t res;

	res = ZSTD_decompressStream (self->zstdstream, &output, &input);
	if (res == 0)
		return G_CONVERTER_FINISHED;
	if (ZSTD_isError (res)) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "can not decompress data: %s",
			     ZSTD_getErrorName (res));
		return G_CONVERTER_ERROR;
	}
	*bytes_read = input.pos;
	*bytes_written = output.pos;
	return G_CONVERTER_CONVERTED;
#else
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "AppStream was not built with Zstd support. Can not decompress data.");
	return G_CONVERTER_ERROR;
#endif
}

static void
as_zstd_decompressor_iface_init (GConverterIface *iface)
{
	iface->convert = as_zstd_decompressor_convert;
	iface->reset = as_zstd_decompressor_reset;
}

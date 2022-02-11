/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020-2022 Matthias Klumpp <mak@debian.org>
 * Copyright (C) 2010 Julian Andres Klode <jak@debian.org>
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

#include "as-vercmp.h"

#include <config.h>
#include <glib.h>

#include "as-enums.h"

/**
 * SECTION:as-vercmp
 * @short_description: Version comparison functions.
 * @include: appstream.h
 *
 * Compare software version numbers.
 */

typedef struct AsVersion {
	const gchar *epoch;
	const gchar *version;
	const gchar *version_end;
	const gchar *revision;
	const gchar *revision_end;
} AsVersion;

/**
 * as_version_parse:
 */
static void
as_version_parse (AsVersion *version, const gchar *v)
{
	const gchar *epoch_end = strchr(v, ':');
	const gchar *version_end = strrchr(v, '-');
	const gchar *complete_end = v + strlen(v);

	version->epoch = (epoch_end == NULL) ? "" : v;
	version->version = (epoch_end == NULL) ? v : epoch_end + 1;
	version->version_end = (version_end == NULL) ? complete_end : version_end;
	version->revision = (version_end == NULL) ? "0" : version_end + 1;
	version->revision_end = ((version_end == NULL) ? version->revision + 1
							: complete_end);
}

/**
 * cmp_number:
 * @a: The first number as a string.
 * @b: The second number as a string.
 * @pa: A pointer to a pointer which will be set to the first non-digit in @a.
 * @pb: A pointer to a pointer which will be set to the first non-digit in @b.
 *
 * Compares two numbers that are represented as strings
 * against each other. Compared to converting to an int
 * and comparing the integers, this has the advantage
 * that it does not cause overflow.
 */
static gint
cmp_number (const gchar *a, const gchar *b, const gchar **pa,
					    const gchar **pb)
{
	gint res = 0;
	if (*a == '\0' && *b == '\0')
		return 0;
	for (; *a == '0'; a++);
	for (; *b == '0'; b++);
	for (; g_ascii_isdigit (*a) && g_ascii_isdigit (*b); a++, b++) {
		if (res == 0 && *a != *b)
			res = *a < *b ? -1 : 1;
	}
	if (g_ascii_isdigit (*a)) {
		if (!g_ascii_isdigit (*b))
			res = 1;
	} else if (g_ascii_isdigit (*b)) {
		res = -1;
	}
	if (pa != NULL) {
		g_assert (pb != NULL);
		*pa = a;
		*pb = b;
	}
	return res;
}

/**
 * cmp_part:
 */
static gint
cmp_part (const gchar *a, const gchar *a_end,
			  const gchar *b, const gchar *b_end)
{
	while (a != a_end || b != b_end) {
		int cmpres;
		for (; !g_ascii_isdigit (*a) || !g_ascii_isdigit (*b); a++, b++) {
			if (a == a_end && b == b_end)
				return 0;
			else if (*a == *b && a != a_end && b != b_end)
				continue;
			/* Tilde always sorts first; i.e., the string with tilde loses */
			else if (*a == '~' || *b == '~')
				return (*a == '~') ? -1 : 1;
			/* One string is empty, other is a number -> go into number mode */
			else if ((a == a_end && *b == '0') || (b == b_end && *a == '0'))
				return cmp_number (a, b, NULL, NULL);
			/* One string is empty, other is not a number -> other wins */
			else if (a == a_end || b == b_end)
				return (a == a_end) ? -1 : 1;
			/* One non-digit part is shorter than the other one */
			else if (g_ascii_isdigit (*a) != g_ascii_isdigit (*b))
				return g_ascii_isdigit(*a) ? -1 : 1;
			/* Alpha looses against not alpha */
			else if (g_ascii_isalpha (*a) != g_ascii_isalpha (*b))
				return g_ascii_isalpha (*a) ? -1 : 1;
			/* Standard ASCII comparison */
			else
				return *a < *b ? -1 : 1;
		}

		/* Now compare numbers */
		cmpres = cmp_number (a, b, &a, &b);
		if (cmpres != 0 || (a == a_end && b == b_end))
			return cmpres;
	}

	return 0;
}

/**
 * as_vercmp:
 * @a: First version number
 * @b: Second version number
 * @flags: Flags, e.g. %AS_VERCMP_FLAG_NONE
 *
 * Compare alpha and numeric segments of two software versions,
 * considering @flags.
 *
 * Returns: >>0 if a is newer than b;
 *	    0 if a and b are the same version;
 *	    <<0 if b is newer than a
 */
gint
as_vercmp (const gchar* a, const gchar *b, AsVercmpFlags flags)
{
	AsVersion ver_a, ver_b;
	gint res = 0;

	if (a == 0 && b == 0)
		return 0;
	if (a == NULL)
		return -1;
	if (b == NULL)
		return 1;

	/* check if a and b are the same pointer */
	if (a == b)
		return 0;

	/* Optimize the case of differing single digit epochs. */
	if (!as_flags_contains (flags, AS_VERCMP_FLAG_IGNORE_EPOCH)) {
		if (*a != *b && *a != '\0' && *b != '\0' && a[1] == ':' && b[1] == ':')
			return *a < *b ? -1 : 1;
	}

	/* easy comparison to see if versions are identical */
	if (g_strcmp0 (a, b) == 0)
		return 0;

	as_version_parse (&ver_a, a);
	as_version_parse (&ver_b, b);

	if (G_UNLIKELY (ver_a.epoch != ver_b.epoch) &&
		(!as_flags_contains (flags, AS_VERCMP_FLAG_IGNORE_EPOCH)) &&
		(res = cmp_number (ver_a.epoch, ver_b.epoch, NULL, NULL)) != 0)
		goto out;
	else if ((res = cmp_part (ver_a.version, ver_a.version_end,
				  ver_b.version, ver_b.version_end)) != 0)
		goto out;
	/* Optimizes for native version numbers (where revision is a string literal) */
	else if (G_LIKELY (ver_a.revision != ver_b.revision) &&
		(res = cmp_part (ver_a.revision, ver_a.revision_end,
				 ver_b.revision, ver_b.revision_end)) != 0)
		goto out;

  out:
	return res;
}

/**
 * as_vercmp_simple:
 * @a: First version number
 * @b: Second version number
 *
 * Compare alpha and numeric segments of two software versions.
 *
 * Returns: >>0 if a is newer than b;
 *	    0 if a and b are the same version;
 *	    <<0 if b is newer than a
 */
gint
as_vercmp_simple (const gchar* a, const gchar *b)
{
	return as_vercmp (a, b, AS_VERCMP_FLAG_NONE);
}

/**
 * as_utils_compare_versions:
 *
 * Compare alpha and numeric segments of two versions.
 * The version compare algorithm is also used by RPM.
 *
 * Returns: 1: a is newer than b
 *	    0: a and b are the same version
 *	   -1: b is newer than a
 */
gint
as_utils_compare_versions (const gchar* a, const gchar *b)
{
	gint r;
	r = as_vercmp (a, b, AS_VERCMP_FLAG_NONE);
	if (r == 0)
		return 0;
	return (r < 0)? -1 : 1;
}

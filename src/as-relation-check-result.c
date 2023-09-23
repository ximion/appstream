/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022-2023 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * SECTION:as-releation-check-result
 * @short_description: Result from a satisfication check on an #AsRelation
 * @include: appstream.h
 *
 * This class contains resulting information from a check for whether an
 * #AsRelation is satisfied on a specific system configuration.
 *
 * See also: #AsRelation
 */

#include "config.h"
#include "as-relation-check-result-private.h"

typedef struct {
	AsRelationStatus status;
	gchar *message;
} AsRelationCheckResultPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsRelationCheckResult, as_relation_check_result, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (as_relation_check_result_get_instance_private (o))

static void
as_relation_check_result_init (AsRelationCheckResult *relcr)
{
	AsRelationCheckResultPrivate *priv = GET_PRIVATE (relcr);

	priv->message = NULL;
}

static void
as_relation_check_result_finalize (GObject *object)
{
	AsRelationCheckResult *relcr = AS_RELATION_CHECK_RESULT (object);
	AsRelationCheckResultPrivate *priv = GET_PRIVATE (relcr);

	g_free (priv->message);

	G_OBJECT_CLASS (as_relation_check_result_parent_class)->finalize (object);
}

static void
as_relation_check_result_class_init (AsRelationCheckResultClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_relation_check_result_finalize;
}

/**
 * as_relation_check_result_new:
 *
 * Creates a new #AsRelationCheckResult.
 *
 * Returns: (transfer full): a #AsRelationCheckResult
 *
 * Since: 1.0.0
 **/
AsRelationCheckResult *
as_relation_check_result_new (void)
{
	AsRelationCheckResult *relcr;
	relcr = g_object_new (AS_TYPE_RELATION_CHECK_RESULT, NULL);
	return AS_RELATION_CHECK_RESULT (relcr);
}

/**
 * as_relation_check_result_get_status:
 * @relcr: an #AsRelationCheckResult instance.
 *
 * Returns the status of this relation check result.
 * If the status is %AS_RELATION_STATUS_ERROR, an error message will
 * have been set as message.
 *
 * Returns: an #AsRelationStatus
 */
AsRelationStatus
as_relation_check_result_get_status (AsRelationCheckResult *relcr)
{
	AsRelationCheckResultPrivate *priv = GET_PRIVATE (relcr);
	return priv->status;
}

/**
 * as_relation_check_result_set_status:
 * @relcr: an #AsRelationCheckResult instance.
 * @status: the new #AsRelationStatus
 *
 * Set the outcome of this relation check result.
 */
void
as_relation_check_result_set_status (AsRelationCheckResult *relcr, AsRelationStatus status)
{
	AsRelationCheckResultPrivate *priv = GET_PRIVATE (relcr);
	priv->status = status;
}

/**
 * as_relation_check_result_get_message:
 * @relcr: an #AsRelationCheckResult instance.
 *
 * Get a human-readable message about the state of this relation.
 * May be %NULL in case the relation is satisfied and there is no further information about it.
 *
 * Returns: (nullable): a human-readable message about this relation's state.
 */
const gchar *
as_relation_check_result_get_message (AsRelationCheckResult *relcr)
{
	AsRelationCheckResultPrivate *priv = GET_PRIVATE (relcr);
	return priv->message;
}

/**
 * as_relation_check_result_set_message:
 * @relcr: an #AsRelationCheckResult instance.
 * @format: the new message
 *
 * Set a human-readable information message about the satisfaction state
 * of the dependency under the checked system configuration.
 */
void
as_relation_check_result_set_message (AsRelationCheckResult *relcr, const gchar *format, ...)
{
	AsRelationCheckResultPrivate *priv = GET_PRIVATE (relcr);
	va_list args;

	g_free (priv->message);
	va_start (args, format);
	priv->message = g_strdup_vprintf (format, args);
	va_end (args);
}

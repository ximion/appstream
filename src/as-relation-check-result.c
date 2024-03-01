/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "as-relation-private.h"

typedef struct {
	AsRelationStatus status;
	AsRelation *relation;

	gchar *message;
	AsRelationError error_code;
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
	if (priv->relation != NULL)
		g_object_unref (priv->relation);

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
 * as_relation_check_result_get_relation:
 * @relcr: an #AsRelationCheckResult instance.
 *
 * Get the relation that this check result was generated for.
 *
 * Returns: (nullable) (transfer none): an #AsRelation or %NULL
 */
AsRelation *
as_relation_check_result_get_relation (AsRelationCheckResult *relcr)
{
	AsRelationCheckResultPrivate *priv = GET_PRIVATE (relcr);
	return priv->relation;
}

/**
 * as_relation_check_result_set_relation:
 * @relcr: an #AsRelationCheckResult instance.
 * @relation: the #AsRelation
 *
 * Set an #AsRelation to associate with this check result.
 */
void
as_relation_check_result_set_relation (AsRelationCheckResult *relcr, AsRelation *relation)
{
	AsRelationCheckResultPrivate *priv = GET_PRIVATE (relcr);
	if (priv->relation != NULL)
		g_object_unref (priv->relation);
	priv->relation = g_object_ref (relation);
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

/**
 * as_relation_check_result_get_error_code:
 * @relcr: an #AsRelationCheckResult instance.
 *
 * Retrieve the error code, in case this result represents an error.
 *
 * Returns: an #AsRelationError
 */
AsRelationError
as_relation_check_result_get_error_code (AsRelationCheckResult *relcr)
{
	AsRelationCheckResultPrivate *priv = GET_PRIVATE (relcr);
	return priv->error_code;
}

/**
 * as_relation_check_result_set_error_code:
 * @relcr: an #AsRelationCheckResult instance.
 * @ecode: the #AsRelationError
 *
 * Set the error code in case this result represents an error.
 */
void
as_relation_check_result_set_error_code (AsRelationCheckResult *relcr, AsRelationError ecode)
{
	AsRelationCheckResultPrivate *priv = GET_PRIVATE (relcr);
	priv->error_code = ecode;
}

/**
 * as_relation_check_results_get_compatibility_score:
 * @rc_results: (element-type AsRelationCheckResult): an array of #AsRelationCheckResult
 *
 * Calculate a compatibility sore between 0 and 100 based on the given set of
 * AsRelationCheckResults.
 *
 * A compatibility of 100 means all requirements are satisfied and the component will
 * run perfectly on the confoguration it was tested agains, while 0 means it will not run at all.
 */
gint
as_relation_check_results_get_compatibility_score (GPtrArray *rc_results)
{
	/* we assume 100% compatibility by default */
	gint score = 100;
	gboolean have_control_supports = FALSE;
	gboolean found_supported_control = FALSE;

	for (guint i = 0; i < rc_results->len; i++) {
		AsRelationCheckResult *rcr = AS_RELATION_CHECK_RESULT (
		    g_ptr_array_index (rc_results, i));
		AsRelationKind rel_kind;
		AsRelationItemKind rel_item_kind;
		AsRelationStatus status;
		AsRelation *rel = as_relation_check_result_get_relation (rcr);

		if (rel == NULL) {
			g_warning ("Missing associated relation for relation-check result entity.");
			continue;
		}

		rel_kind = as_relation_get_kind (rel);
		rel_item_kind = as_relation_get_item_kind (rel);
		status = as_relation_check_result_get_status (rcr);

		/* Anything that is required and not fulfilled will give an instant 0% compatibility,
		 * if we don't know the status of a required element, we give a strong penality. */
		if (rel_kind == AS_RELATION_KIND_REQUIRES) {
			if (status == AS_RELATION_STATUS_UNKNOWN)
				score -= 30;
			else if (status != AS_RELATION_STATUS_SATISFIED)
				return 0;

			/* if we are here, the requirement is satisfied, and if it is an input control,
			 * we recognize an input control is available */
			if (rel_item_kind == AS_RELATION_ITEM_KIND_CONTROL) {
				have_control_supports = TRUE;
				found_supported_control = TRUE;
			}

			continue;
		}

		/* Missing recommended items get a penalty */
		if (rel_kind == AS_RELATION_KIND_RECOMMENDS) {
			/* for compatibility, we treat recommends a bit like supports with regards to controls */
			if (rel_item_kind == AS_RELATION_ITEM_KIND_CONTROL)
				have_control_supports = TRUE;

			if (status != AS_RELATION_STATUS_SATISFIED) {
				score -= 10;
				if (rel_item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH) {
					/* reduce score again if the recommendation was for display */
					score -= 20;
				}
			} else if (rel_item_kind == AS_RELATION_ITEM_KIND_CONTROL) {
				found_supported_control = TRUE;
				score += 5;
			}
		}

		/* Increase score in case a supported item is present */
		if (rel_kind == AS_RELATION_KIND_SUPPORTS) {
			/* controls are special - if we have *none* of the supported/required/recommended
			 * controls, that is a pretty big issue, but as long as one is supported we are good */
			if (rel_item_kind == AS_RELATION_ITEM_KIND_CONTROL) {
				have_control_supports = TRUE;
				if (status == AS_RELATION_STATUS_SATISFIED) {
					score += 4;
					found_supported_control = TRUE;
				}
			} else {
				if (status == AS_RELATION_STATUS_SATISFIED)
					score += 2;
			}
		}
	}

	/* If we have controls defined, but the configuration has no control that was listed as
	 * supported, controlling this software will be difficult. We add a huge penalty for that. */
	if (have_control_supports && !found_supported_control)
		score -= 60;

	if (score < 0)
		return 0;
	if (score > 100)
		return 100;
	return score;
}

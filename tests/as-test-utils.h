
#ifndef __AS_TEST_UTILS_H
#define __AS_TEST_UTILS_H

#include <glib-object.h>
#include <appstream.h>

G_BEGIN_DECLS

gboolean	as_test_compare_lines (const gchar *txt1,
				       const gchar *txt2);

void 		as_component_sort_values (AsComponent *cpt);
void 		as_sort_components (GPtrArray *cpts);
void 		as_sort_strings (GPtrArray *utf8);

GBytes		*as_gbytes_from_literal (const gchar *string);

G_END_DECLS

#endif /* __AS_TEST_UTILS_H */


#include "as-test-utils.h"

/**
 * as_test_compare_lines:
 **/
gboolean
as_test_compare_lines (const gchar *txt1, const gchar *txt2)
{
	g_autoptr(GError) error = NULL;
	g_autofree gchar *output = NULL;

	/* exactly the same */
	if (g_strcmp0 (txt1, txt2) == 0)
		return TRUE;

	/* save temp files and diff them */
	if (!g_file_set_contents ("/tmp/as-utest_a", txt1, -1, &error))
		return FALSE;
	if (!g_file_set_contents ("/tmp/as-utest_b", txt2, -1, &error))
		return FALSE;
	if (!g_spawn_command_line_sync ("diff -urNp /tmp/as-utest_b /tmp/as-utest_a",
					&output, NULL, NULL, &error))
		return FALSE;

	g_assert_no_error (error);
	g_print ("%s\n", output);
	return FALSE;
}

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <QStringList>
#include <QByteArray>
#include "qt/chelpers.h"

START_TEST(test_stringListToCharArray_no_overflow)
{
    // Invariant: Buffer reads never exceed the declared length
    const char *payloads[] = {
        "A",  // Valid minimal input
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  // Boundary: 50 chars
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"  // Exploit: 100 chars
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        QStringList list;
        list.append(QString::fromUtf8(payloads[i]));
        
        char **array = stringListToCharArray(list);
        
        // Check null terminator was written within bounds
        ck_assert_ptr_nonnull(array);
        ck_assert_ptr_null(array[list.size()]);
        
        // Verify string content matches
        ck_assert_str_eq(array[0], payloads[i]);
        
        // Cleanup
        for (int j = 0; j < list.size(); j++) {
            g_free(array[j]);
        }
        g_free(array);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_stringListToCharArray_no_overflow);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
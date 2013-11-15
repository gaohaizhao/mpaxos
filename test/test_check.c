
#include <stdlib.h>
#include <check.h>

#include "test_hostname.c"


START_TEST (lalala)
{
    ck_assert_int_lt(1, 2);
    ck_assert_int_eq(3, 3);
}
END_TEST

START_TEST(hahaha) {
    char a[] = "hello";
    ck_assert_str_eq(a, "hello");
}END_TEST


Suite *check_suite (void) {
    Suite *s = suite_create("MPaxos");

    TCase *tc_core = tcase_create("Demo");
    tcase_add_test(tc_core, lalala);
    tcase_add_test(tc_core, hahaha);
    suite_add_tcase(s, tc_core);

    TCase *tc_util = tcase_create("Util");
    tcase_add_test(tc_util, hostname);
    suite_add_tcase(s, tc_util);


    return s;
}

int main() {
    int number_failed;
    Suite *s = NULL;
    SRunner *sr = NULL;

    s = check_suite();
    sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

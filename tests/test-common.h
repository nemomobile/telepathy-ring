#ifndef _TEST_COMMON_H
#define _TEST_COMMON_H

#include <glib.h>
#include <check.h>

struct common_args {
  char *xml;         /* file to save XML check output to */
  GSList *tests;     /* list of tests to run;
                      * empty list means run all tests */
};

struct common_args *parse_common_args(int argc, char *const argv[]);
void free_common_args(struct common_args *args);


typedef TCase *(*tcase_func)(void);

struct test_cases {
  tcase_func tcase_callback;
  const char *tcase_name;
  int default_on;
};

#define DECLARE_TEST_CASE(tc_func)                                      \
  { .tcase_name = (#tc_func), .tcase_callback = (tc_func), .default_on = 1 }
#define DECLARE_TEST_CASE_OFF_BY_DEFAULT(tc_func)                       \
  { .tcase_name = (#tc_func), .tcase_callback = (tc_func), .default_on = 0 }
#define LAST_TEST_CASE                                  \
  { .tcase_name = NULL, .tcase_callback = NULL }

void filter_add_tcases(Suite *suite, struct test_cases *tests, GSList *filter);

#endif

/*
 * Copyright (C) 2009  Nokia Corporation.  All rights reserved.
 * @author Andres Salomon <dilinger@collabora.co.uk>
 */

#include "config.h"
#include "test-common.h"

#include <glib.h>
#include <check.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(int err, const char *argv0)
{
  fprintf(err == EXIT_FAILURE ? stderr : stdout,
    "Usage: %s [args]\n"
    "[args]:\n"
    "  -h                    display this help menu\n"
    "  -o <file>             XML log (check format)\n"
    "  -t <testcase>         which test case to run\n"
    "             (-t may be listed multiple times)\n"
    "\n", argv0);
  exit(err);
}

struct common_args *parse_common_args(int argc, char *const argv[])
{
  int opt, all_tests = 0;
  struct common_args *args;

  args = calloc(1, sizeof(*args));
  if (!args)
    return NULL;

  while ((opt = getopt(argc, argv, "ho:t:")) != -1) {
    switch (opt) {
      case 'h':
        usage(EXIT_SUCCESS, argv[0]);
        break;

      case 'o':
        args->xml = strdup(optarg);
        break;

      case 't':
        /* check for magic strings that tell us to run all tests */
        if (strcmp(optarg, "*") == 0 || strcmp(optarg, "all") == 0) {
          all_tests = 1;
          if (args->tests) {
            g_slist_foreach(args->tests, (GFunc) g_free, NULL);
            g_slist_free(args->tests);
            args->tests = NULL;
          }
        }
        if (all_tests)
          break;

        /* only running some tests; pay attention to which ones */
        args->tests = g_slist_prepend(args->tests, strdup(optarg));
        break;

      default:
        usage(EXIT_FAILURE, argv[0]);
    }
  }

  return args;
}

void free_common_args(struct common_args *args)
{
  if (args->xml)
    free(args->xml);
  g_slist_foreach(args->tests, (GFunc) g_free, NULL);
  g_slist_free(args->tests);
  free(args);
}

void filter_add_tcases(Suite *suite, struct test_cases *tests, GSList *filter)
{
  struct test_cases *i;

  for (i = tests; i->tcase_callback != NULL; i++) {
    if ((!filter && i->default_on) ||
      g_slist_find_custom(filter, i->tcase_name,
        (GCompareFunc) g_ascii_strcasecmp))
      suite_add_tcase(suite, i->tcase_callback());
    else
      printf("skipping test %s\n", i->tcase_name);
  }
}

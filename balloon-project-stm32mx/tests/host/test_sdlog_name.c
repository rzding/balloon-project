/**
 * @file test_sdlog_name.c
 * @brief Host unit tests for FLIGHTxxx.CSV name / next-index helpers (F6.2).
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "sdlog.h"

static int failures;

static void assert_true(bool got, const char *msg)
{
  if (!got)
  {
    printf("FAIL %s\n", msg);
    failures++;
  }
}

static void assert_int(int got, int expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %d expected %d\n", msg, got, expected);
    failures++;
  }
}

static void assert_str(const char *got, const char *expected, const char *msg)
{
  if (got == NULL || expected == NULL || strcmp(got, expected) != 0)
  {
    printf("FAIL %s: got \"%s\" expected \"%s\"\n", msg,
           got ? got : "(null)", expected ? expected : "(null)");
    failures++;
  }
}

/* Bit i set => FLIGHT%03u.CSV exists. */
static bool mock_exists(const char *name, void *ctx)
{
  const unsigned *mask = (const unsigned *)ctx;
  unsigned i;
  char expect[SDLOG_FLIGHT_NAME_MAX];

  for (i = 0; i < 32u; i++)
  {
    if (!sdlog_format_flight_name(i, expect, sizeof(expect)))
    {
      return false;
    }
    if (strcmp(name, expect) == 0)
    {
      return ((*mask) & (1u << i)) != 0u;
    }
  }
  return false;
}

static bool mock_all_exist(const char *name, void *ctx)
{
  (void)name;
  (void)ctx;
  return true;
}

int main(void)
{
  char buf[SDLOG_FLIGHT_NAME_MAX];
  unsigned mask;

  failures = 0;

  assert_true(sdlog_format_flight_name(0, buf, sizeof(buf)), "format 0 ok");
  assert_str(buf, "FLIGHT000.CSV", "format 0");

  assert_true(sdlog_format_flight_name(42, buf, sizeof(buf)), "format 42 ok");
  assert_str(buf, "FLIGHT042.CSV", "format 42");

  assert_true(sdlog_format_flight_name(999, buf, sizeof(buf)), "format 999 ok");
  assert_str(buf, "FLIGHT999.CSV", "format 999");

  assert_true(!sdlog_format_flight_name(1000, buf, sizeof(buf)), "format 1000 reject");
  assert_true(!sdlog_format_flight_name(0, buf, 12), "buflen 12 reject");
  assert_true(!sdlog_format_flight_name(0, NULL, sizeof(buf)), "null buf reject");

  mask = 0u;
  assert_int(sdlog_next_flight_index(mock_exists, &mask), 0, "empty card -> 0");

  mask = 0x7u; /* 0,1,2 exist */
  assert_int(sdlog_next_flight_index(mock_exists, &mask), 3, "gap after 0..2 -> 3");

  assert_int(sdlog_next_flight_index(NULL, NULL), -1, "null exists -> -1");
  assert_int(sdlog_next_flight_index(mock_all_exist, NULL), -1, "all exist -> -1");

  if (failures == 0)
  {
    printf("PASS test_sdlog_name (%d checks conceptually)\n", 12);
    return 0;
  }

  printf("%d failure(s)\n", failures);
  return 1;
}

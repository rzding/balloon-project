/**
 * @file test_aprs_at.c
 * @brief Host unit tests for DRA818V AT format / ACK parse helpers (F10.1).
 */

#include <stdio.h>
#include <string.h>

#include "aprs.h"

static int failures = 0;

static void assert_true(int cond, const char *msg)
{
  if (!cond)
  {
    printf("FAIL %s\n", msg);
    failures++;
  }
}

static void assert_streq(const char *got, const char *expected, const char *msg)
{
  if (got == NULL || expected == NULL || strcmp(got, expected) != 0)
  {
    printf("FAIL %s: got \"%s\" expected \"%s\"\n", msg,
           got ? got : "(null)", expected ? expected : "(null)");
    failures++;
  }
}

int main(void)
{
  char buf[APRS_AT_BUF_LEN];
  int n;

  n = aprs_format_dmosetgroup(buf, sizeof(buf), APRS_GBW, APRS_FREQ_MHZ_STR,
                              APRS_FREQ_MHZ_STR, APRS_CTCSS_NONE, APRS_SQUELCH,
                              APRS_CTCSS_NONE);
  assert_true(n > 0, "setgroup len");
  assert_streq(buf, "AT+DMOSETGROUP=0,144.3900,144.3900,0000,4,0000\r\n",
               "setgroup locked");

  n = aprs_format_dmosetvolume(buf, sizeof(buf), APRS_VOLUME);
  assert_true(n > 0, "volume len");
  assert_streq(buf, "AT+DMOSETVOLUME=8\r\n", "volume locked");

  n = aprs_format_setfilter(buf, sizeof(buf), APRS_FILTER_OFF, APRS_FILTER_OFF,
                            APRS_FILTER_OFF);
  assert_true(n > 0, "filter len");
  assert_streq(buf, "AT+SETFILTER=1,1,1\r\n", "filter locked");

  assert_true(aprs_format_dmosetvolume(buf, sizeof(buf), 0) < 0, "vol 0 bad");
  assert_true(aprs_format_dmosetvolume(buf, sizeof(buf), 9) < 0, "vol 9 bad");
  assert_true(aprs_format_dmosetgroup(buf, sizeof(buf), 0, "144.3900",
                                      "144.3900", "0000", 9, "0000") < 0,
              "sq 9 bad");
  assert_true(aprs_format_dmosetgroup(buf, 8, 0, "144.3900", "144.3900",
                                      "0000", 4, "0000") < 0,
              "setgroup overflow");

  assert_true(aprs_at_ack_ok("+DMOCONNECT:0", "+DMOCONNECT"), "connect ok");
  assert_true(aprs_at_ack_ok("+DMOCONNECT: 0\r", "+DMOCONNECT"),
              "connect spaced");
  assert_true(aprs_at_ack_ok("+DMOSETGROUP:0", "+DMOSETGROUP"), "group ok");
  assert_true(aprs_at_ack_ok("+DMOSETVOLUME:0", "+DMOSETVOLUME"), "vol ok");
  assert_true(aprs_at_ack_ok("+DMOSETFILTER:0", "+DMOSETFILTER"), "filt ok");
  assert_true(!aprs_at_ack_ok("+DMOCONNECT:1", "+DMOCONNECT"), "connect fail");
  assert_true(!aprs_at_ack_ok("+DMOSETGROUP:0", "+DMOCONNECT"), "wrong tag");
  assert_true(!aprs_at_ack_ok(NULL, "+DMOCONNECT"), "null resp");
  assert_true(!aprs_at_ack_ok("+DMOCONNECT:0", NULL), "null tag");

  if (failures == 0)
  {
    printf("All APRS AT tests passed.\n");
    return 0;
  }

  printf("%d test(s) failed.\n", failures);
  return 1;
}

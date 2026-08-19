/**
 * @file test_gps_rx.c
 * @brief Host unit tests for GPS ring buffer and LF line extraction (F5.1).
 */

#include <stdio.h>
#include <string.h>

#include "gps.h"

static int failures = 0;

static void assert_bool(bool got, bool expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %d expected %d\n", msg, (int)got, (int)expected);
    failures++;
  }
}

static void assert_str(const char *got, const char *expected, const char *msg)
{
  if (strcmp(got, expected) != 0)
  {
    printf("FAIL %s: got \"%s\" expected \"%s\"\n", msg, got, expected);
    failures++;
  }
}

/** Drain all bytes from ring through line accumulator; return last complete line. */
static bool drain_ring(gps_ring_t *ring, gps_line_acc_t *acc, char *out, size_t out_cap)
{
  uint8_t byte;
  bool got_line = false;
  char line_buf[GPS_LINE_MAX];

  while (gps_ring_pop(ring, &byte))
  {
    if (gps_line_feed(acc, byte, line_buf, sizeof(line_buf)))
    {
      memcpy(out, line_buf, out_cap);
      got_line = true;
    }
  }

  return got_line;
}

static void push_bytes(gps_ring_t *ring, const char *s)
{
  while (*s != '\0')
  {
    gps_ring_push(ring, (uint8_t)*s);
    s++;
  }
}

int main(void)
{
  gps_ring_t ring;
  gps_line_acc_t acc;
  char line[GPS_LINE_MAX];
  uint16_t i;

  memset(&ring, 0, sizeof(ring));
  memset(&acc, 0, sizeof(acc));

  /* Single NMEA-like line ending CRLF. */
  push_bytes(&ring, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");
  assert_bool(drain_ring(&ring, &acc, line, sizeof(line)), true, "one line extracted");
  assert_str(line, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47", "crlf stripped");

  /* Two lines across sequential drains (models two gps_poll cycles). */
  memset(&ring, 0, sizeof(ring));
  memset(&acc, 0, sizeof(acc));
  push_bytes(&ring, "$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62\r\n");
  assert_bool(drain_ring(&ring, &acc, line, sizeof(line)), true, "first line drain");
  assert_str(line, "$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62", "first line content");
  push_bytes(&ring, "$GPGGA,081836,3751.65,S,14507.36,E,1,03,24,19.7,M,,,,0000*1F\r\n");
  assert_bool(drain_ring(&ring, &acc, line, sizeof(line)), true, "second line drain");
  assert_str(line, "$GPGGA,081836,3751.65,S,14507.36,E,1,03,24,19.7,M,,,,0000*1F", "second line content");

  /* Two lines in one drain: last complete line wins (gps_poll semantics). */
  memset(&ring, 0, sizeof(ring));
  memset(&acc, 0, sizeof(acc));
  push_bytes(&ring, "$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62\r\n");
  push_bytes(&ring, "$GPGGA,081836,3751.65,S,14507.36,E,1,03,24,19.7,M,,,,0000*1F\r\n");
  assert_bool(drain_ring(&ring, &acc, line, sizeof(line)), true, "both lines one drain");
  assert_str(line, "$GPGGA,081836,3751.65,S,14507.36,E,1,03,24,19.7,M,,,,0000*1F", "last line wins");

  /* Partial line across two drains. */
  memset(&ring, 0, sizeof(ring));
  memset(&acc, 0, sizeof(acc));
  push_bytes(&ring, "$GPGGA,partial");
  assert_bool(drain_ring(&ring, &acc, line, sizeof(line)), false, "partial no line yet");
  push_bytes(&ring, ",rest\r\n");
  assert_bool(drain_ring(&ring, &acc, line, sizeof(line)), true, "partial completed");
  assert_str(line, "$GPGGA,partial,rest", "partial joined");

  /* Ring wrap: flood past capacity, then deliver a valid short line. */
  memset(&ring, 0, sizeof(ring));
  memset(&acc, 0, sizeof(acc));
  for (i = 0u; i < (GPS_RING_SIZE + 16u); i++)
  {
    gps_ring_push(&ring, (uint8_t)('A' + (i % 26u)));
  }
  push_bytes(&ring, "\n$WRAP,ok\r\n");
  assert_bool(drain_ring(&ring, &acc, line, sizeof(line)), true, "wrap still yields line");
  assert_str(line, "$WRAP,ok", "wrap line content");

  /* Oversize line discarded; next well-formed line accepted. */
  memset(&ring, 0, sizeof(ring));
  memset(&acc, 0, sizeof(acc));
  for (i = 0u; i < (GPS_LINE_MAX + 8u); i++)
  {
    gps_ring_push(&ring, (uint8_t)'X');
  }
  gps_ring_push(&ring, (uint8_t)'\n');
  assert_bool(drain_ring(&ring, &acc, line, sizeof(line)), false, "oversize discarded");
  push_bytes(&ring, "$GNRMC,ok\r\n");
  assert_bool(drain_ring(&ring, &acc, line, sizeof(line)), true, "after oversize ok");
  assert_str(line, "$GNRMC,ok", "after oversize content");

  /* Empty drain returns no line. */
  memset(&ring, 0, sizeof(ring));
  memset(&acc, 0, sizeof(acc));
  assert_bool(drain_ring(&ring, &acc, line, sizeof(line)), false, "empty drain");

  if (failures == 0)
  {
    printf("PASS test_gps_rx (%d checks)\n", 16);
    return 0;
  }

  printf("FAIL test_gps_rx: %d failure(s)\n", failures);
  return 1;
}

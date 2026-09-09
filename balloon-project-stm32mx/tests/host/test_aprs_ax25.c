/**
 * @file test_aprs_ax25.c
 * @brief Host unit tests for AX.25 UI / APRS position encode (F10.2).
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

static void assert_u16(uint16_t got, uint16_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got 0x%04X expected 0x%04X\n", msg,
           (unsigned)got, (unsigned)expected);
    failures++;
  }
}

static void assert_size(size_t got, size_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %zu expected %zu\n", msg, got, expected);
    failures++;
  }
}

int main(void)
{
  char buf[APRS_INFO_MAX];
  uint8_t frame[APRS_AX25_FRAME_MAX];
  uint8_t bits[APRS_AX25_BIT_MAX];
  size_t flen = 0u;
  size_t blen;
  uint8_t addr[7];
  int n;

  /* San Francisco-ish sample: 37.7749 N, 122.4194 W, 100 m. */
  n = aprs_format_lat_aprs(buf, sizeof(buf), 377749000);
  assert_true(n == 8, "lat len");
  assert_streq(buf, "3746.49N", "lat fmt");

  n = aprs_format_lon_aprs(buf, sizeof(buf), -1224194000);
  assert_true(n == 9, "lon len");
  assert_streq(buf, "12225.16W", "lon fmt");

  assert_true(aprs_alt_m_to_feet(100) == 328, "100 m → feet");
  assert_true(aprs_alt_m_to_feet(0) == 0, "0 m → feet");

  n = aprs_format_info_field(buf, sizeof(buf), 377749000, -1224194000, 100);
  assert_true(n == 29, "info len");
  assert_streq(buf, "!3746.49N/12225.16WO/A=000328", "info field");

  n = aprs_format_info_field(buf, sizeof(buf), 0, 0, 0);
  assert_true(n > 0, "info zero ok");
  assert_streq(buf, "!0000.00N/00000.00EO/A=000000", "info zero");

  assert_true(aprs_ax25_encode_addr(addr, "APZSSI", 0u, false), "enc dest");
  assert_true(addr[0] == 0x82u && addr[5] == 0x92u && addr[6] == 0x60u,
              "dest bytes");
  assert_true(aprs_ax25_encode_addr(addr, "N0CALL", 11u, false), "enc src");
  assert_true(addr[6] == 0x76u, "src ssid 11");
  assert_true(aprs_ax25_encode_addr(addr, "WIDE2", 1u, true), "enc digi");
  assert_true(addr[6] == 0x63u, "digi last+ssid1");

  {
    const char *info = "!3746.49N/12225.16WO/A=000328";
    flen = aprs_ax25_build_ui(frame, sizeof(frame), info, strlen(info));
    assert_size(flen, 54u, "ui frame len");
    assert_true(frame[21] == APRS_AX25_CTRL_UI && frame[22] == APRS_AX25_PID_NOL3,
                "ctrl pid");
    assert_u16(aprs_ax25_fcs(frame, flen - 2u), 0x14ADu, "fcs value");
    assert_true(frame[flen - 2u] == 0xADu && frame[flen - 1u] == 0x14u,
                "fcs LE");
  }

  blen = aprs_build_position_frame(frame, sizeof(frame), &flen, bits,
                                   sizeof(bits), 377749000, -1224194000, 100);
  assert_true(blen > 0u, "bitstream nonempty");
  assert_size(flen, 54u, "build frame len");
  /* Leading flags alone: 16 * 8 = 128 NRZI bits minimum. */
  assert_true(blen > 128u, "bitstream has flags+body");
  assert_true(bits[0] == 0u || bits[0] == 1u, "nrzi bit 0/1");

  assert_true(aprs_format_lat_aprs(buf, 4u, 0) < 0, "lat overflow");
  assert_true(aprs_ax25_build_ui(frame, 8u, "x", 1u) == 0u, "frame overflow");
  assert_true(aprs_build_position_frame(NULL, 0u, NULL, NULL, 0u, 0, 0, 0) == 0u,
              "bits required");

  if (failures == 0)
  {
    printf("All APRS AX.25 tests passed.\n");
    return 0;
  }

  printf("%d test(s) failed.\n", failures);
  return 1;
}

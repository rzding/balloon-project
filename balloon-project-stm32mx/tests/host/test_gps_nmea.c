/**
 * @file test_gps_nmea.c
 * @brief Host unit tests for NMEA GGA/RMC parse (F5.2).
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

static void assert_i32(int32_t got, int32_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %ld expected %ld\n", msg, (long)got, (long)expected);
    failures++;
  }
}

static void assert_u8(uint8_t got, uint8_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %u expected %u\n", msg, (unsigned)got, (unsigned)expected);
    failures++;
  }
}

static void assert_i16(int16_t got, int16_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %d expected %d\n", msg, (int)got, (int)expected);
    failures++;
  }
}

static void assert_u32(uint32_t got, uint32_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %lu expected %lu\n", msg, (unsigned long)got, (unsigned long)expected);
    failures++;
  }
}

int main(void)
{
  gps_sample_t patch;
  gps_sample_t state;
  int32_t e7;

  /* Checksum validation. */
  assert_bool(gps_nmea_checksum_ok("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"),
              true,
              "gga checksum ok");
  assert_bool(gps_nmea_checksum_ok("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00"),
              false,
              "gga checksum bad");

  /* Sentence type (talker-agnostic). */
  assert_bool(gps_nmea_sentence_type("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47") ==
                  GPS_NMEA_GGA,
              true,
              "gpgga type");
  assert_bool(gps_nmea_sentence_type("$GNRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*7C") ==
                  GPS_NMEA_RMC,
              true,
              "gnrmc type");
  assert_bool(gps_nmea_sentence_type("$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48") == GPS_NMEA_NONE, true, "vtg none");

  /* ddmm → e7 integer math. */
  assert_bool(gps_nmea_ddmm_to_e7("4807.038", true, 'N', &e7), true, "ddmm lat ok");
  assert_i32(e7, 481173000, "ddmm lat e7");
  assert_bool(gps_nmea_ddmm_to_e7("01131.000", false, 'E', &e7), true, "ddmm lon ok");
  assert_i32(e7, 115166666, "ddmm lon e7");
  assert_bool(gps_nmea_ddmm_to_e7("3751.65", true, 'S', &e7), true, "ddmm south ok");
  assert_i32(e7, -378608333, "ddmm south e7");
  assert_bool(gps_nmea_ddmm_to_e7("14507.36", false, 'E', &e7), true, "ddmm lon2 ok");
  assert_i32(e7, 1451226666, "ddmm lon2 e7");

  /* GGA golden sentence. */
  memset(&patch, 0, sizeof(patch));
  assert_bool(gps_nmea_parse_sentence(
                  "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47",
                  &patch),
              true,
              "parse gpgga");
  assert_bool(patch.lat_lon_valid, true, "gga lat_lon_valid");
  assert_i32(patch.lat_e7, 481173000, "gga lat");
  assert_i32(patch.lon_e7, 115166666, "gga lon");
  assert_u8(patch.fix_quality, 1u, "gga fix quality");
  assert_u8(patch.sats, 8u, "gga sats");
  assert_bool(patch.alt_valid, true, "gga alt valid");
  assert_i16(patch.alt_m, 545, "gga alt");
  assert_u32(patch.time_hhmmss, 123519u, "gga time");

  /* RMC valid (GP talker). */
  memset(&patch, 0, sizeof(patch));
  assert_bool(gps_nmea_parse_sentence(
                  "$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62",
                  &patch),
              true,
              "parse gprmc");
  assert_bool(patch.rmc_valid, true, "rmc valid A");
  assert_bool(patch.lat_lon_valid, true, "rmc lat_lon");
  assert_i32(patch.lat_e7, -378608333, "rmc lat");
  assert_i32(patch.lon_e7, 1451226666, "rmc lon");
  assert_u32(patch.time_hhmmss, 81836u, "rmc time");
  assert_u32(patch.date_ddmmyy, 130998u, "rmc date");

  /* RMC invalid status V. */
  memset(&patch, 0, sizeof(patch));
  assert_bool(gps_nmea_parse_sentence(
                  "$GPRMC,081836,V,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*75",
                  &patch),
              true,
              "parse rmc V");
  assert_bool(patch.rmc_valid, false, "rmc valid V");

  /* GN talker GGA. */
  memset(&patch, 0, sizeof(patch));
  assert_bool(gps_nmea_parse_sentence(
                  "$GNGGA,081836,3751.65,S,14507.36,E,1,03,24,19.7,M,,,,0000*03",
                  &patch),
              true,
              "parse gngga");
  assert_u8(patch.sats, 3u, "gngga sats");
  assert_i16(patch.alt_m, 19, "gngga alt");

  /* Unknown sentence ignored. */
  memset(&patch, 0, sizeof(patch));
  assert_bool(gps_nmea_parse_sentence("$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48", &patch), false, "vtg parse");

  /* Merge: GGA then RMC into state. */
  memset(&state, 0, sizeof(state));
  memset(&patch, 0, sizeof(patch));
  assert_bool(gps_nmea_parse_gga("$GNGGA,081836,3751.65,S,14507.36,E,1,03,24,19.7,M,,,,0000*03", &patch),
              true,
              "merge gga parse");
  gps_nmea_merge_patch(&state, &patch, true);
  memset(&patch, 0, sizeof(patch));
  assert_bool(gps_nmea_parse_rmc("$GNRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*7C", &patch),
              true,
              "merge rmc parse");
  gps_nmea_merge_patch(&state, &patch, false);
  assert_bool(state.lat_lon_valid, true, "merge lat_lon");
  assert_bool(state.alt_valid, true, "merge alt from gga");
  assert_i16(state.alt_m, 19, "merge alt value");
  assert_bool(state.rmc_valid, true, "merge rmc valid");
  assert_u32(state.date_ddmmyy, 130998u, "merge date");

  if (failures == 0)
  {
    printf("PASS test_gps_nmea (%d checks)\n", 38);
    return 0;
  }

  printf("FAIL test_gps_nmea: %d failure(s)\n", failures);
  return 1;
}

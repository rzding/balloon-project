/**
 * @file test_packetizer.c
 * @brief Host unit tests for F8.3 packetizer fill + pack/CRC.
 */

#include <stdio.h>
#include <string.h>

#include "packetizer.h"

static int failures = 0;

static void assert_true(bool got, const char *msg)
{
  if (!got)
  {
    printf("FAIL %s\n", msg);
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

static void assert_u16(uint16_t got, uint16_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got 0x%04X expected 0x%04X\n", msg,
           (unsigned)got, (unsigned)expected);
    failures++;
  }
}

static void assert_u32(uint32_t got, uint32_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %lu expected %lu\n", msg,
           (unsigned long)got, (unsigned long)expected);
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

static void assert_i16(int16_t got, int16_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %d expected %d\n", msg, (int)got, (int)expected);
    failures++;
  }
}

static void test_fill_fields(void)
{
  packetizer_sample_t in;
  packet_v1_t out;

  memset(&in, 0, sizeof(in));
  in.mission_state = 2u;
  in.seq = 42u;
  in.time_ms = 12345678u;
  /* flags OK byte 0x05 => fault mask low byte 0xFA */
  in.error_flags = 0xFAu;
  in.baro_valid = true;
  in.baro_alt_m = 1480.0f;
  in.baro_temp_centi_c = 2000;
  in.temp_valid = true;
  in.temp_centi_c = 2350;
  in.gps_sample_valid = true;
  in.sats = 8u;
  in.lat_lon_valid = true;
  in.lat_e7 = 377749200;
  in.lon_e7 = -122419400;
  in.gps_alt_valid = true;
  in.gps_alt_m = 1500u;

  packetizer_fill(&in, &out);

  assert_u8(out.version, PACKET_V1_VERSION, "version");
  assert_u8(out.mission_state, 2u, "mission_state");
  assert_u16(out.seq, 42u, "seq");
  assert_u32(out.time_ms, 12345678u, "time_ms");
  assert_i32(out.lat_e7, 377749200, "lat_e7");
  assert_i32(out.lon_e7, -122419400, "lon_e7");
  assert_u16(out.gps_alt_m, 1500u, "gps_alt_m");
  assert_i16(out.baro_alt_m, 1480, "baro_alt_m");
  assert_i16(out.temp_c_x100, 2350, "temp overrides baro");
  assert_u16(out.batt, PACKET_V1_BATT_NA, "batt N/A");
  assert_u8(out.flags, 0x05u, "flags OK polarity");
  assert_u8(out.sats, 8u, "sats");
}

static void test_temp_override_and_defaults(void)
{
  packetizer_sample_t in;
  packet_v1_t out;

  memset(&in, 0, sizeof(in));
  in.baro_valid = true;
  in.baro_alt_m = 100.0f;
  in.baro_temp_centi_c = 1111;
  packetizer_fill(&in, &out);
  assert_i16(out.temp_c_x100, 1111, "baro temp when no MAX31865");

  in.temp_valid = true;
  in.temp_centi_c = 2222;
  packetizer_fill(&in, &out);
  assert_i16(out.temp_c_x100, 2222, "MAX31865 overrides");

  memset(&in, 0, sizeof(in));
  in.error_flags = 0u; /* all OK */
  packetizer_fill(&in, &out);
  assert_u8(out.flags, 0xFFu, "all healthy flags");
  assert_u16(out.batt, PACKET_V1_BATT_NA, "empty batt N/A");
  assert_i16(out.baro_alt_m, 0, "no baro zero");
  assert_u8(out.sats, 0u, "no gps sats");
}

static void test_fill_pack_golden(void)
{
  packetizer_sample_t in;
  packet_v1_t fields;
  packet_v1_t out;
  uint8_t wire[PACKET_V1_LEN];
  /* Same rich vector as test_packet_v1 golden hex */
  static const uint8_t golden[PACKET_V1_LEN] = {
      0x01, 0x02, 0x00, 0x2a, 0x00, 0xbc, 0x61, 0x4e,
      0x16, 0x83, 0xfe, 0xd0, 0xf8, 0xb4, 0x07, 0x38,
      0x05, 0xdc, 0x05, 0xc8, 0x09, 0x2e, 0xff, 0xff,
      0x05, 0x08, 0x6e, 0xcf
  };

  memset(&in, 0, sizeof(in));
  in.mission_state = 2u;
  in.seq = 42u;
  in.time_ms = 12345678u;
  in.error_flags = 0xFAu;
  in.baro_valid = true;
  in.baro_alt_m = 1480.0f;
  in.baro_temp_centi_c = 2000;
  in.temp_valid = true;
  in.temp_centi_c = 2350;
  in.gps_sample_valid = true;
  in.sats = 8u;
  in.lat_lon_valid = true;
  in.lat_e7 = 377749200;
  in.lon_e7 = -122419400;
  in.gps_alt_valid = true;
  in.gps_alt_m = 1500u;

  packetizer_fill(&in, &fields);
  packetizer_pack(&fields, wire);

  assert_true(memcmp(wire, golden, PACKET_V1_LEN) == 0, "fill→pack golden wire");
  assert_u16(packet_be_u16_read(&wire[26]), 0x6ECFu, "golden CRC");
  assert_true(packet_v1_unpack(wire, &out), "unpack fill golden");
  assert_u8(out.mission_state, 2u, "round-trip state");
  assert_i16(out.temp_c_x100, 2350, "round-trip temp");
}

int main(void)
{
  test_fill_fields();
  test_temp_override_and_defaults();
  test_fill_pack_golden();

  if (failures == 0)
  {
    printf("PASS test_packetizer\n");
    return 0;
  }
  printf("%d failure(s)\n", failures);
  return 1;
}

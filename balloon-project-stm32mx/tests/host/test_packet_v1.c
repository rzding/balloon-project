/**
 * @file test_packet_v1.c
 * @brief Host unit tests for telemetry packet v1 pack/unpack/CRC (F7.4).
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "packet.h"

static int failures = 0;

static void assert_true(bool got, const char *msg)
{
  if (!got)
  {
    printf("FAIL %s\n", msg);
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
    printf("FAIL %s: got %ld expected %ld\n", msg,
           (long)got, (long)expected);
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

static void assert_u8(uint8_t got, uint8_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %u expected %u\n", msg, (unsigned)got, (unsigned)expected);
    failures++;
  }
}

int main(void)
{
  packet_v1_t in;
  packet_v1_t out;
  uint8_t buf[PACKET_V1_LEN];
  uint16_t crc;

  memset(&in, 0, sizeof(in));
  in.version = PACKET_V1_VERSION;
  in.mission_state = 2u;
  in.seq = 42u;
  in.time_ms = 12345678u;
  in.lat_e7 = 377749200;
  in.lon_e7 = -122419400;
  in.gps_alt_m = 1500u;
  in.baro_alt_m = 1480;
  in.temp_c_x100 = 2350;
  in.batt = PACKET_V1_BATT_NA;
  in.flags = 0x05u;
  in.sats = 8u;

  packet_v1_pack(&in, buf);

  crc = packet_crc16(buf, PACKET_V1_CRC_LEN);
  assert_u16(packet_be_u16_read(&buf[26]), crc, "packed CRC matches packet_crc16");
  assert_u16(crc, 0x6ECFu, "golden CRC for rich vector");

  assert_true(packet_v1_unpack(buf, &out), "unpack rich vector");
  assert_u8(out.version, PACKET_V1_VERSION, "version");
  assert_u8(out.mission_state, 2u, "mission_state");
  assert_u16(out.seq, 42u, "seq");
  assert_u32(out.time_ms, 12345678u, "time_ms");
  assert_i32(out.lat_e7, 377749200, "lat_e7");
  assert_i32(out.lon_e7, -122419400, "lon_e7");
  assert_u16(out.gps_alt_m, 1500u, "gps_alt_m");
  assert_i16(out.baro_alt_m, 1480, "baro_alt_m");
  assert_i16(out.temp_c_x100, 2350, "temp_c_x100");
  assert_u16(out.batt, PACKET_V1_BATT_NA, "batt");
  assert_u8(out.flags, 0x05u, "flags");
  assert_u8(out.sats, 8u, "sats");

  buf[10] ^= 0x01u;
  assert_true(!packet_v1_unpack(buf, &out), "corrupt byte fails unpack");
  packet_v1_pack(&in, buf);

  buf[0] = 0x02u;
  assert_true(!packet_v1_unpack(buf, &out), "wrong version fails unpack");

  memset(&in, 0, sizeof(in));
  in.batt = PACKET_V1_BATT_NA;
  packet_v1_pack(&in, buf);
  /* CRC over 26 bytes: 0100000000000000000000000000000000000000ffff0000 (not 23-byte truncated hex). */
  assert_u8(buf[22], 0xFFu, "minimal batt high");
  assert_u8(buf[23], 0xFFu, "minimal batt low");
  assert_u16(packet_be_u16_read(&buf[26]), 0x18EFu, "minimal vector CRC");
  assert_true(packet_v1_unpack(buf, &out), "minimal vector unpack");

  if (failures == 0)
  {
    printf("All packet v1 tests passed.\n");
    printf("Golden hex (rich): 0102002a00bc614e1683fed0f8b4073805dc05c8092effff05086ecf\n");
    return 0;
  }

  printf("%d test(s) failed.\n", failures);
  return 1;
}

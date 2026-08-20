/**
 * @file decode_packet.c
 * @brief Ground-station CLI decoder for telemetry packet v1 (F7.4).
 *
 * Usage:
 *   decode_packet <56-hex-chars> [--rssi <dBm>] [--snr <dB>]
 *
 * RSSI/SNR come from the RX radio (RegPktRssiValue / RegPktSnrValue) when
 * available; pass via flags or log as n/a until Nucleo RX firmware exists.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "packet.h"

static int hex_nibble(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f')
  {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F')
  {
    return 10 + (c - 'A');
  }
  return -1;
}

static bool parse_hex_payload(const char *hex, uint8_t out[PACKET_V1_LEN])
{
  size_t len;
  size_t i;

  if (hex == NULL || out == NULL)
  {
    return false;
  }

  len = strlen(hex);
  if (len != (size_t)(PACKET_V1_LEN * 2u))
  {
    return false;
  }

  for (i = 0u; i < PACKET_V1_LEN; i++)
  {
    int hi;
    int lo;

    hi = hex_nibble(hex[i * 2u]);
    lo = hex_nibble(hex[i * 2u + 1u]);
    if (hi < 0 || lo < 0)
    {
      return false;
    }

    out[i] = (uint8_t)((hi << 4) | lo);
  }

  return true;
}

static void print_usage(const char *prog)
{
  fprintf(stderr,
          "Usage: %s <56-hex-chars> [--rssi <dBm>] [--snr <dB>]\n"
          "Decode 28-byte packet v1 (big-endian, CRC-16/CCITT-FALSE).\n",
          prog);
}

int main(int argc, char **argv)
{
  uint8_t wire[PACKET_V1_LEN];
  packet_v1_t pkt;
  const char *hex = NULL;
  const char *rssi_str = "n/a";
  const char *snr_str = "n/a";
  bool ok;
  int i;

  if (argc < 2)
  {
    print_usage(argv[0]);
    return 1;
  }

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--rssi") == 0)
    {
      if (i + 1 >= argc)
      {
        fprintf(stderr, "error: --rssi requires a value\n");
        return 1;
      }
      rssi_str = argv[++i];
    }
    else if (strcmp(argv[i], "--snr") == 0)
    {
      if (i + 1 >= argc)
      {
        fprintf(stderr, "error: --snr requires a value\n");
        return 1;
      }
      snr_str = argv[++i];
    }
    else if (argv[i][0] == '-')
    {
      fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
      print_usage(argv[0]);
      return 1;
    }
    else if (hex == NULL)
    {
      hex = argv[i];
    }
    else
    {
      fprintf(stderr, "error: unexpected argument '%s'\n", argv[i]);
      print_usage(argv[0]);
      return 1;
    }
  }

  if (hex == NULL)
  {
    print_usage(argv[0]);
    return 1;
  }

  if (!parse_hex_payload(hex, wire))
  {
    fprintf(stderr, "error: payload must be exactly %u hex characters\n",
            (unsigned)(PACKET_V1_LEN * 2u));
    return 1;
  }

  ok = packet_v1_unpack(wire, &pkt);

  printf("crc_ok=%s\n", ok ? "true" : "false");
  printf("version=0x%02X\n", (unsigned)pkt.version);
  printf("mission_state=%u\n", (unsigned)pkt.mission_state);
  printf("seq=%u\n", (unsigned)pkt.seq);
  printf("time_ms=%lu\n", (unsigned long)pkt.time_ms);
  printf("lat_e7=%ld\n", (long)pkt.lat_e7);
  printf("lon_e7=%ld\n", (long)pkt.lon_e7);
  printf("gps_alt_m=%u\n", (unsigned)pkt.gps_alt_m);
  printf("baro_alt_m=%d\n", (int)pkt.baro_alt_m);
  printf("temp_c_x100=%d\n", (int)pkt.temp_c_x100);
  printf("batt=0x%04X\n", (unsigned)pkt.batt);
  printf("flags=0x%02X\n", (unsigned)pkt.flags);
  printf("sats=%u\n", (unsigned)pkt.sats);
  printf("rssi=%s\n", rssi_str);
  printf("snr=%s\n", snr_str);

  return ok ? 0 : 1;
}

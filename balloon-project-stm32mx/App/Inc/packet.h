/**
 * @file packet.h
 * @brief Telemetry packet v1 wire format (28 bytes, big-endian, CRC-16/CCITT-FALSE).
 *
 * F7.4: shared contract for flight TX (F8 packetizer) and ground decode.
 * Header-only — no packet.c in firmware until F8 calls pack from app_run.
 *
 * CRC: poly 0x1021, init 0xFFFF, refin/refout false, xorout 0x0000 (CCITT-FALSE).
 * CRC covers bytes 0–25; stored big-endian at offset 26.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Wire length of packet v1 (bytes). */
#define PACKET_V1_LEN           28u

/** Locked packet version byte. */
#define PACKET_V1_VERSION       0x01u

/** Battery field when not measured (N/A). */
#define PACKET_V1_BATT_NA       0xFFFFu

/** Number of payload bytes covered by CRC (excludes CRC field). */
#define PACKET_V1_CRC_LEN       26u

/** Decoded packet v1 fields (host-endian). */
typedef struct
{
  uint8_t version;
  uint8_t mission_state;
  uint16_t seq;
  uint32_t time_ms;
  int32_t lat_e7;
  int32_t lon_e7;
  uint16_t gps_alt_m;
  int16_t baro_alt_m;
  int16_t temp_c_x100;
  uint16_t batt;
  uint8_t flags;
  uint8_t sats;
} packet_v1_t;

static inline uint16_t packet_be_u16_read(const uint8_t *buf)
{
  return (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
}

static inline uint32_t packet_be_u32_read(const uint8_t *buf)
{
  return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
         ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

static inline int32_t packet_be_i32_read(const uint8_t *buf)
{
  return (int32_t)packet_be_u32_read(buf);
}

static inline int16_t packet_be_i16_read(const uint8_t *buf)
{
  return (int16_t)packet_be_u16_read(buf);
}

static inline void packet_be_u16_write(uint8_t *buf, uint16_t v)
{
  buf[0] = (uint8_t)(v >> 8);
  buf[1] = (uint8_t)v;
}

static inline void packet_be_u32_write(uint8_t *buf, uint32_t v)
{
  buf[0] = (uint8_t)(v >> 24);
  buf[1] = (uint8_t)(v >> 16);
  buf[2] = (uint8_t)(v >> 8);
  buf[3] = (uint8_t)v;
}

static inline void packet_be_i32_write(uint8_t *buf, int32_t v)
{
  packet_be_u32_write(buf, (uint32_t)v);
}

static inline void packet_be_i16_write(uint8_t *buf, int16_t v)
{
  packet_be_u16_write(buf, (uint16_t)v);
}

/**
 * @brief CRC-16/CCITT-FALSE over arbitrary byte span.
 *
 * @param data Input bytes (may be NULL only if len is 0).
 * @param len Byte count.
 * @return CRC-16 value.
 */
static inline uint16_t packet_crc16(const uint8_t *data, uint16_t len)
{
  uint16_t crc = 0xFFFFu;
  uint16_t i;
  uint8_t bit;

  for (i = 0u; i < len; i++)
  {
    crc ^= (uint16_t)((uint16_t)data[i] << 8);
    for (bit = 0u; bit < 8u; bit++)
    {
      if ((crc & 0x8000u) != 0u)
      {
        crc = (uint16_t)((crc << 1) ^ 0x1021u);
      }
      else
      {
        crc = (uint16_t)(crc << 1);
      }
    }
  }

  return crc;
}

/**
 * @brief Pack packet v1 into 28-byte wire buffer (big-endian + CRC).
 *
 * @param in Source fields (version forced to PACKET_V1_VERSION on wire).
 * @param out Output buffer (PACKET_V1_LEN bytes).
 */
static inline void packet_v1_pack(const packet_v1_t *in, uint8_t out[PACKET_V1_LEN])
{
  uint16_t crc;

  if (in == NULL || out == NULL)
  {
    return;
  }

  out[0] = PACKET_V1_VERSION;
  out[1] = in->mission_state;
  packet_be_u16_write(&out[2], in->seq);
  packet_be_u32_write(&out[4], in->time_ms);
  packet_be_i32_write(&out[8], in->lat_e7);
  packet_be_i32_write(&out[12], in->lon_e7);
  packet_be_u16_write(&out[16], in->gps_alt_m);
  packet_be_i16_write(&out[18], in->baro_alt_m);
  packet_be_i16_write(&out[20], in->temp_c_x100);
  packet_be_u16_write(&out[22], in->batt);
  out[24] = in->flags;
  out[25] = in->sats;

  crc = packet_crc16(out, PACKET_V1_CRC_LEN);
  packet_be_u16_write(&out[26], crc);
}

/**
 * @brief Unpack and validate packet v1 from wire buffer.
 *
 * @param in Input buffer (PACKET_V1_LEN bytes).
 * @param out Decoded fields (only written on success).
 * @return false on NULL args, version mismatch, or CRC failure.
 */
static inline bool packet_v1_unpack(const uint8_t in[PACKET_V1_LEN], packet_v1_t *out)
{
  uint16_t crc_stored;
  uint16_t crc_calc;

  if (in == NULL || out == NULL)
  {
    return false;
  }

  if (in[0] != PACKET_V1_VERSION)
  {
    return false;
  }

  crc_stored = packet_be_u16_read(&in[26]);
  crc_calc = packet_crc16(in, PACKET_V1_CRC_LEN);
  if (crc_stored != crc_calc)
  {
    return false;
  }

  out->version = in[0];
  out->mission_state = in[1];
  out->seq = packet_be_u16_read(&in[2]);
  out->time_ms = packet_be_u32_read(&in[4]);
  out->lat_e7 = packet_be_i32_read(&in[8]);
  out->lon_e7 = packet_be_i32_read(&in[12]);
  out->gps_alt_m = packet_be_u16_read(&in[16]);
  out->baro_alt_m = packet_be_i16_read(&in[18]);
  out->temp_c_x100 = packet_be_i16_read(&in[20]);
  out->batt = packet_be_u16_read(&in[22]);
  out->flags = in[24];
  out->sats = in[25];

  return true;
}

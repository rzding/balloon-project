/**
 * @file packetizer.h
 * @brief Fill packet v1 fields from samples + pack/CRC (F8.3).
 *
 * Pure logic — no HAL. Reuses packet.h wire helpers for CRC-16/CCITT-FALSE.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "packet.h"

/**
 * @brief Injected telemetry snapshot for one packetizer_fill.
 *
 * Sensor I/O stays in app; this struct is host-testable.
 */
typedef struct
{
  uint8_t mission_state;
  uint16_t seq;
  uint32_t time_ms;
  /** Raw fault bitfield (error_flags_get polarity: bit set = fault). */
  uint32_t error_flags;
  bool baro_valid;
  float baro_alt_m;
  int16_t baro_temp_centi_c;
  bool temp_valid;
  int16_t temp_centi_c;
  bool gps_sample_valid;
  uint8_t sats;
  bool lat_lon_valid;
  int32_t lat_e7;
  int32_t lon_e7;
  bool gps_alt_valid;
  uint16_t gps_alt_m;
} packetizer_sample_t;

/**
 * @brief Fill @p out from @p in (flags OK polarity, temp overrides baro die temp).
 *
 * @param in Non-NULL sample.
 * @param out Non-NULL destination fields.
 */
void packetizer_fill(const packetizer_sample_t *in, packet_v1_t *out);

/**
 * @brief Pack fields to wire bytes including CRC-16 (wrapper around packet_v1_pack).
 */
void packetizer_pack(const packet_v1_t *fields, uint8_t wire[PACKET_V1_LEN]);

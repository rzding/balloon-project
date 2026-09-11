/**
 * @file telem_imu.h
 * @brief IMU telemetry packet (type 0x02) — raw accel + gyro over LoRa.
 *
 * The 28-byte v1 beacon (packet.h) has no IMU fields; rather than break that
 * frozen format, IMU rides its own packet, distinguished by the type byte at
 * offset 0. The ground station dispatches on in[0]: 0x01 v1 telemetry, 0x02 IMU.
 *
 * Wire layout (22 bytes, big-endian, CRC-16/CCITT-FALSE over bytes 0..19):
 *   [0]     type = 0x02
 *   [1]     mission_state
 *   [2..3]  seq
 *   [4..7]  time_ms
 *   [8..9]  ax   [10..11] ay  [12..13] az   (raw LSB, 2048 LSB/g)
 *   [14..15] gx  [16..17] gy  [18..19] gz   (raw LSB, 16.4 LSB/dps)
 *   [20..21] CRC-16
 *
 * Header-only, reuses packet.h wire + CRC helpers, no HAL.
 */

#pragma once

#include <stdint.h>

#include "imu.h"
#include "packet.h"

#define TELEM_IMU_TYPE      0x02u
#define TELEM_IMU_LEN       22u
#define TELEM_IMU_CRC_LEN   20u

/** @brief Pack an IMU sample into the 22-byte wire buffer (with CRC). */
static inline void telem_imu_pack(uint8_t out[TELEM_IMU_LEN], uint8_t mission_state,
                                  uint16_t seq, uint32_t time_ms, const imu_sample_t *s)
{
  uint16_t crc;

  if (out == NULL || s == NULL)
  {
    return;
  }

  out[0] = TELEM_IMU_TYPE;
  out[1] = mission_state;
  packet_be_u16_write(&out[2], seq);
  packet_be_u32_write(&out[4], time_ms);
  packet_be_i16_write(&out[8], s->ax);
  packet_be_i16_write(&out[10], s->ay);
  packet_be_i16_write(&out[12], s->az);
  packet_be_i16_write(&out[14], s->gx);
  packet_be_i16_write(&out[16], s->gy);
  packet_be_i16_write(&out[18], s->gz);

  crc = packet_crc16(out, TELEM_IMU_CRC_LEN);
  packet_be_u16_write(&out[20], crc);
}

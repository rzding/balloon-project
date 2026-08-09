/**
 * @file baro.h
 * @brief MS5611-01BA03 barometer driver (SPI1 + BARO_CS).
 *
 * F3.1: SPI reset, PROM read (C1–C6), datasheet CRC4 (AN520).
 * F3.2–F3.4: conversion, compensation, baro_read — not yet implemented.
 *
 * MS5611 uses command-byte SPI (not IMU bit-7 R/W); use spi_bus_transfer directly.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Number of 16-bit PROM words (manufacturer + C1..C6 + serial/CRC). */
#define BARO_PROM_WORD_COUNT  8u

/** PROM index of coefficient C1 (1-based naming in datasheet). */
#define BARO_PROM_C1_INDEX    1u

/** PROM index of coefficient C6. */
#define BARO_PROM_C6_INDEX    6u

/** PROM index of serial/CRC word (low nibble = CRC4). */
#define BARO_PROM_CRC_INDEX   7u

/**
 * @brief Compute MS5611 PROM CRC4 nibble per AN520 / datasheet.
 *
 * Operates on a local copy; does not modify @p prom.
 *
 * @param prom Eight 16-bit PROM words as read from the device (MSB first).
 * @return Computed 4-bit CRC.
 */
static inline uint8_t baro_crc4(const uint16_t prom[BARO_PROM_WORD_COUNT])
{
  uint16_t work[BARO_PROM_WORD_COUNT];
  uint8_t cnt;
  uint8_t n_bit;
  uint16_t n_rem;
  uint8_t i;

  for (i = 0u; i < BARO_PROM_WORD_COUNT; i++)
  {
    work[i] = prom[i];
  }

  work[BARO_PROM_CRC_INDEX] = (uint16_t)(work[BARO_PROM_CRC_INDEX] & 0xFF00u);

  n_rem = 0x00u;
  for (cnt = 0u; cnt < 16u; cnt++)
  {
    if ((cnt % 2u) == 1u)
    {
      n_rem ^= (uint16_t)(work[cnt >> 1] & 0x00FFu);
    }
    else
    {
      n_rem ^= (uint16_t)(work[cnt >> 1] >> 8);
    }

    for (n_bit = 8u; n_bit > 0u; n_bit--)
    {
      if ((n_rem & 0x8000u) != 0u)
      {
        n_rem = (uint16_t)((n_rem << 1) ^ 0x3000u);
      }
      else
      {
        n_rem = (uint16_t)(n_rem << 1);
      }
    }
  }

  return (uint8_t)((n_rem >> 12) & 0x000Fu);
}

/**
 * @brief Validate PROM CRC4 against the stored nibble in word 7.
 *
 * @param prom Eight 16-bit PROM words as read from the device.
 * @return true if computed CRC matches prom[7] low nibble; false otherwise.
 */
static inline bool baro_prom_crc_ok(const uint16_t prom[BARO_PROM_WORD_COUNT])
{
  uint8_t crc_stored = (uint8_t)(prom[BARO_PROM_CRC_INDEX] & 0x000Fu);
  return baro_crc4(prom) == crc_stored;
}

/**
 * @brief Reset MS5611, read PROM, verify CRC4, cache calibration coefficients.
 *
 * Sends reset (0x1E), waits for PROM reload (>= 2.8 ms), reads all eight PROM
 * words, validates CRC4 and non-zero C1–C6. Updates error_flags on success or failure.
 *
 * @return false on SPI failure, invalid PROM, or CRC mismatch; true on success.
 */
bool baro_init(void);

/**
 * @brief Last known barometer health after baro_init.
 * @return true if last baro_init succeeded; false otherwise.
 */
bool baro_is_ok(void);

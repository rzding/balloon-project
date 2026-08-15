/**
 * @file baro.h
 * @brief MS5611-01BA03 barometer driver (SPI1 + BARO_CS).
 *
 * F3.1: SPI reset, PROM read (C1–C6), datasheet CRC4 (AN520).
 * F3.2: D1/D2 conversion @ OSR 4096, timed wait, 24-bit ADC read (baro_read_raw).
 * F3.3: first-/second-order compensation, ISA altitude helper (host-testable).
 * F3.4: baro_read — not yet implemented.
 *
 * Locked conversion default (F3.2): OSR 4096 (D1 0x48, D2 0x58); max conversion 9.04 ms.
 *
 * MS5611 uses command-byte SPI (not IMU bit-7 R/W); use spi_bus_transfer directly.
 */

#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/** Sea-level pressure in ICAO ISA (Pa). */
#define BARO_ISA_P0_PA           101325.0f

/** Troposphere / isothermal boundary pressure (Pa). */
#define BARO_ISA_P11_PA          22632.1f

/** Isothermal / stratosphere boundary pressure at 20 km (Pa). */
#define BARO_ISA_P20_PA          5474.9f

/** Geopotential altitude at tropopause (m). */
#define BARO_ISA_H11_M           11000.0f

/** Geopotential altitude at 20 km (m). */
#define BARO_ISA_H20_M           20000.0f

/** Specific gas constant for dry air (J/(kg·K)). */
#define BARO_ISA_R_AIR           287.05287f

/** Standard gravity (m/s²). */
#define BARO_ISA_G0              9.80665f

/** ISA temperature at 11 km (K). */
#define BARO_ISA_T11_K           216.65f

/** ISA lapse rate 11–20 km (K/m). */
#define BARO_ISA_LAPSE_20_32     (-0.001f)

/** Number of 16-bit PROM words (manufacturer + C1..C6 + serial/CRC). */
#define BARO_PROM_WORD_COUNT  8u

/** PROM index of coefficient C1 (1-based naming in datasheet). */
#define BARO_PROM_C1_INDEX    1u

/** PROM index of coefficient C2. */
#define BARO_PROM_C2_INDEX    2u

/** PROM index of coefficient C3. */
#define BARO_PROM_C3_INDEX    3u

/** PROM index of coefficient C4. */
#define BARO_PROM_C4_INDEX    4u

/** PROM index of coefficient C5. */
#define BARO_PROM_C5_INDEX    5u

/** PROM index of coefficient C6. */
#define BARO_PROM_C6_INDEX    6u

/** PROM index of serial/CRC word (low nibble = CRC4). */
#define BARO_PROM_CRC_INDEX   7u

/** Raw pressure (D1) and temperature (D2) ADC values (24-bit, MSB first from device). */
typedef struct
{
  uint32_t d1;
  uint32_t d2;
} baro_raw_t;

/** Compensated pressure/temperature (datasheet integer units). */
typedef struct
{
  int32_t temp_centi_c; /**< Temperature in 0.01 °C (datasheet TEMP). */
  int32_t pressure_pa;  /**< Pressure in Pa (datasheet P: 0.01 mbar = 1 Pa). */
} baro_comp_t;

/**
 * @brief Combine big-endian ADC bytes into unsigned 24-bit sample.
 * @param b0 MSB (first data byte after command).
 * @param b1 Middle byte.
 * @param b2 LSB.
 */
static inline uint32_t baro_be_bytes_to_u24(uint8_t b0, uint8_t b1, uint8_t b2)
{
  return ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | (uint32_t)b2;
}

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
 * @brief Apply MS5611 first-/second-order compensation (datasheet B3 integer math).
 *
 * Uses prom[C1..C6] with raw D1 (pressure) and D2 (temperature) ADC values.
 * Host-testable; no HAL dependency.
 *
 * @param prom Eight PROM words (uses indices C1–C6).
 * @param d1   Pressure ADC (24-bit).
 * @param d2   Temperature ADC (24-bit).
 * @param out  Compensated result; must not be NULL.
 * @return false on NULL @p prom/@p out or zero @p d1/@p d2; true on success.
 */
static inline bool baro_compensate(const uint16_t prom[BARO_PROM_WORD_COUNT],
                                   uint32_t d1, uint32_t d2, baro_comp_t *out)
{
  int64_t dt;
  int64_t off;
  int64_t sens;
  int32_t temp;
  int32_t t2;
  int32_t off2;
  int32_t sens2;
  int64_t p;
  uint32_t c1;
  uint32_t c2;
  uint16_t c3;
  uint16_t c4;
  int32_t c5;
  uint16_t c6;

  if (prom == NULL || out == NULL || d1 == 0u || d2 == 0u)
  {
    return false;
  }

  c1 = (uint32_t)prom[BARO_PROM_C1_INDEX];
  c2 = (uint32_t)prom[BARO_PROM_C2_INDEX];
  c3 = prom[BARO_PROM_C3_INDEX];
  c4 = prom[BARO_PROM_C4_INDEX];
  c5 = (int32_t)prom[BARO_PROM_C5_INDEX];
  c6 = prom[BARO_PROM_C6_INDEX];

  dt = (int64_t)d2 - ((int64_t)c5 * 256);
  temp = (int32_t)(2000 + ((dt * (int64_t)c6) >> 23));

  off = ((int64_t)c2 << 16) + ((dt * (int64_t)c4) >> 7);
  sens = ((int64_t)c1 << 15) + ((dt * (int64_t)c3) >> 8);

  if (temp < 2000)
  {
    t2 = (int32_t)((dt * dt) >> 31);
    off2 = (int32_t)(((int64_t)5 * (int64_t)(temp - 2000) * (int64_t)(temp - 2000)) >> 1);
    sens2 = (int32_t)(((int64_t)5 * (int64_t)(temp - 2000) * (int64_t)(temp - 2000)) >> 2);

    if (temp < -1500)
    {
      off2 = (int32_t)((int64_t)off2 +
                       (int64_t)7 * (int64_t)(temp + 1500) * (int64_t)(temp + 1500));
      sens2 = (int32_t)((int64_t)sens2 +
                          (((int64_t)11 * (int64_t)(temp + 1500) * (int64_t)(temp + 1500)) >> 1));
    }

    temp = temp - t2;
    off = off - (int64_t)off2;
    sens = sens - (int64_t)sens2;
  }

  p = (((int64_t)d1 * sens) >> 21) - off;
  p = p >> 15;

  out->temp_centi_c = temp;
  out->pressure_pa = (int32_t)p;
  return true;
}

/**
 * @brief Compensated temperature in degrees Celsius.
 */
static inline float baro_comp_temp_c(const baro_comp_t *comp)
{
  if (comp == NULL)
  {
    return 0.0f;
  }

  return (float)comp->temp_centi_c / 100.0f;
}

/**
 * @brief Compensated pressure in hectopascals (hPa = mbar).
 */
static inline float baro_comp_pressure_hpa(const baro_comp_t *comp)
{
  if (comp == NULL)
  {
    return 0.0f;
  }

  return (float)comp->pressure_pa / 100.0f;
}

/**
 * @brief Barometric altitude from pressure using ICAO ISA (m).
 *
 * Three layers: troposphere 0–11 km, isothermal 11–20 km, stratosphere 20–32 km.
 */
static inline float baro_pressure_pa_to_alt_m(float pressure_pa)
{
  float h;

  if (pressure_pa <= 0.0f)
  {
    return 0.0f;
  }

  if (pressure_pa >= BARO_ISA_P11_PA)
  {
    h = 44330.77f * (1.0f - powf(pressure_pa / BARO_ISA_P0_PA, 0.190263f));
  }
  else if (pressure_pa >= BARO_ISA_P20_PA)
  {
    h = BARO_ISA_H11_M +
        ((BARO_ISA_R_AIR * BARO_ISA_T11_K) / BARO_ISA_G0) *
            logf(BARO_ISA_P11_PA / pressure_pa);
  }
  else
  {
    const float exponent = (BARO_ISA_R_AIR * BARO_ISA_LAPSE_20_32) / BARO_ISA_G0;

    h = BARO_ISA_H20_M +
        (BARO_ISA_T11_K / (-BARO_ISA_LAPSE_20_32)) *
            (1.0f - powf(pressure_pa / BARO_ISA_P20_PA, exponent));
  }

  return h;
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
 * @brief Poll D1 (pressure) and D2 (temperature) ADCs at OSR 4096.
 *
 * Triggers D1 then D2 conversions with datasheet max wait (HAL_Delay 10 ms each),
 * reads 24-bit results via ADC command 0x00. Updates health on success or failure.
 * Compensation is F3.3; use baro_read_raw for raw ADC only.
 *
 * @param out Out sample; must not be NULL.
 * @return false on NULL @p out, SPI failure, or zero ADC; true on success.
 */
bool baro_read_raw(baro_raw_t *out);

/**
 * @brief Last known barometer health after baro_init or last baro_read_raw.
 * @return true if last init/read succeeded; false otherwise.
 */
bool baro_is_ok(void);

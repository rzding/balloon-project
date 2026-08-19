/**
 * @file temp.h
 * @brief MAX31865 RTD-to-digital driver (SPI1 + Temp_CS, PT1000 3-wire).
 *
 * F4.1: VBIAS on, 3-wire mode, 60 Hz filter, normally-off (1-shot ready for F4.2).
 * F4.2: RTD read and resistance → °C (Callendar–Van Dusen).
 * F4.3: temp_read API and fault handling.
 *
 * Locked init defaults (F4.1):
 *   - VBIAS on, 3-wire, 60 Hz notch, conversion mode normally off (no auto-heat).
 *   - Reference resistor RREF = 4300 Ω per PCB R21 (used in F4.2 math).
 *
 * Locked conversion default (F4.2): 1-shot @ 60 Hz filter; max wait 60 ms.
 *
 * MAX31865 uses IMU/LoRa bit-7 SPI R/W via spi_bus_read_reg8 / spi_bus_write_reg8.
 */

#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/** Reference resistor on PCB (R21), ohms. */
#define TEMP_RREF_OHM           4300.0f

/** PT1000 nominal resistance at 0 °C (IEC 60751), ohms. */
#define TEMP_PT1000_R0_OHM      1000.0f

/** Callendar–Van Dusen coefficients (IEC 60751). */
#define TEMP_CVD_A              3.9083e-3f
#define TEMP_CVD_B              (-5.775e-7f)
#define TEMP_CVD_C              (-4.183e-12f)

/** MAX31865 RTD ADC full-scale code (15-bit). */
#define TEMP_RTD_ADC_FULL_SCALE 32768.0f

/** Raw RTD conversion result from MSB/LSB registers. */
typedef struct
{
  uint16_t adc;         /**< 15-bit RTD ADC code (bits 15:1 of RTD word). */
  bool fault;           /**< RTD LSB bit 0 — hardware fault (open/short). */
  uint8_t fault_status; /**< Fault Status register 0x07; 0 if not read. */
} temp_raw_t;

/**
 * @brief Unpack MAX31865 RTD MSB/LSB into ADC code and fault bit.
 *
 * @param msb RTD MSB register value.
 * @param lsb RTD LSB register value (bit 0 = fault).
 * @param out Out raw sample; must not be NULL.
 * @return false on NULL @p out; true on success.
 */
static inline bool temp_rtd_unpack(uint8_t msb, uint8_t lsb, temp_raw_t *out)
{
  uint16_t raw;

  if (out == NULL)
  {
    return false;
  }

  raw = (uint16_t)(((uint16_t)msb << 8) | (uint16_t)lsb);
  out->adc = (uint16_t)(raw >> 1);
  out->fault = (lsb & 0x01u) != 0u;
  out->fault_status = 0u;
  return true;
}

/**
 * @brief Convert 15-bit RTD ADC code to RTD resistance (ohms).
 *
 * @param adc 15-bit code (0..32767).
 */
static inline float temp_rtd_adc_to_ohm(uint16_t adc)
{
  return ((float)adc / TEMP_RTD_ADC_FULL_SCALE) * TEMP_RREF_OHM;
}

/**
 * @brief Forward CVD resistance from temperature (°C) — host-testable.
 */
static inline float temp_pt1000_c_to_ohm(float temp_c)
{
  float t = temp_c;

  if (t >= 0.0f)
  {
    return TEMP_PT1000_R0_OHM * (1.0f + (TEMP_CVD_A * t) + (TEMP_CVD_B * t * t));
  }

  return TEMP_PT1000_R0_OHM *
         (1.0f + (TEMP_CVD_A * t) + (TEMP_CVD_B * t * t) +
          (TEMP_CVD_C * (t - 100.0f) * t * t * t));
}

/**
 * @brief Inverse CVD: PT1000 resistance (ohms) → temperature (°C).
 *
 * T >= 0: quadratic inverse. T < 0: Newton–Raphson on full IEC equation.
 * Host-testable; no HAL dependency.
 *
 * @param rt_ohm RTD resistance in ohms.
 * @param temp_c Out temperature in °C; must not be NULL.
 * @return false on NULL @p temp_c, non-positive @p rt_ohm, or invalid math.
 */
static inline bool temp_pt1000_ohm_to_c(float rt_ohm, float *temp_c)
{
  float ratio;
  float disc;
  float t;
  uint8_t i;

  if (temp_c == NULL || rt_ohm <= 0.0f)
  {
    return false;
  }

  ratio = rt_ohm / TEMP_PT1000_R0_OHM;

  disc = (TEMP_CVD_A * TEMP_CVD_A) - (4.0f * TEMP_CVD_B * (1.0f - ratio));
  if (disc < 0.0f)
  {
    return false;
  }

  /* B < 0: (-A + sqrt(disc)) / (2B) gives correct sign for T >= 0 and T < 0. */
  t = (-TEMP_CVD_A + sqrtf(disc)) / (2.0f * TEMP_CVD_B);

  if (ratio < 1.0f)
  {
    for (i = 0u; i < 8u; i++)
    {
      float r_model = temp_pt1000_c_to_ohm(t);
      float err = r_model - rt_ohm;
      float t3 = t * t * t;
      float deriv = TEMP_PT1000_R0_OHM *
                    (TEMP_CVD_A + (2.0f * TEMP_CVD_B * t) +
                     (TEMP_CVD_C * ((4.0f * t3) - (300.0f * t * t))));

      if (fabsf(deriv) < 1.0e-6f)
      {
        return false;
      }

      t = t - (err / deriv);

      if (fabsf(err) < 0.001f)
      {
        break;
      }
    }
  }

  *temp_c = t;
  return true;
}

/** Compensated temperature sample for mission / telemetry (packet temp_c_x100). */
typedef struct
{
  int16_t temp_centi_c; /**< Temperature in 0.01 °C (LoRa packet v1 field). */
} temp_sample_t;

/**
 * @brief Build compensated sample from raw RTD ADC (host-testable).
 *
 * @param raw Raw sample from temp_read_raw (must not be faulted).
 * @param out Out sample; must not be NULL.
 * @return false on NULL @p raw/@p out, fault, zero ADC, CVD failure, or overflow; true on success.
 */
static inline bool temp_sample_from_raw(const temp_raw_t *raw, temp_sample_t *out)
{
  float rt_ohm;
  float temp_c;
  long centi;

  if (raw == NULL || out == NULL)
  {
    return false;
  }

  if (raw->fault || raw->adc == 0u)
  {
    return false;
  }

  rt_ohm = temp_rtd_adc_to_ohm(raw->adc);
  if (!temp_pt1000_ohm_to_c(rt_ohm, &temp_c))
  {
    return false;
  }

  centi = lroundf(temp_c * 100.0f);
  if (centi < (long)INT16_MIN || centi > (long)INT16_MAX)
  {
    return false;
  }

  out->temp_centi_c = (int16_t)centi;
  return true;
}

/**
 * @brief Configure MAX31865 for 3-wire PT1000 and verify register read-back.
 *
 * Writes CONFIG (VBIAS | 3WIRE | fault clear), waits for bias settle, then
 * read-back verifies sticky bits. Updates error_flags on success or failure.
 *
 * @return false on SPI failure or config read-back mismatch; true on success.
 */
bool temp_init(void);

/**
 * @brief Poll RTD ADC via 1-shot conversion (60 Hz filter).
 *
 * Triggers 1-shot, waits datasheet max conversion time, burst-reads RTD MSB/LSB.
 * Rejects FAULT bit, zero ADC, or SPI failure. Updates health on success or failure.
 * Compensation to °C is F4.3; use temp_read_raw for raw ADC only.
 *
 * @param out Out sample; must not be NULL.
 * @return false on NULL @p out, SPI failure, fault, or zero ADC; true on success.
 */
bool temp_read_raw(temp_raw_t *out);

/**
 * @brief Poll compensated outside-air temperature in centi-degrees Celsius.
 *
 * Calls temp_read_raw, then converts RTD resistance to °C via CVD.
 * Updates health on success or failure. Not called from app_run until mission (F8).
 *
 * @param out Out sample; must not be NULL.
 * @return false on NULL @p out, SPI failure, fault, zero ADC, or conversion failure; true on success.
 */
bool temp_read(temp_sample_t *out);

/**
 * @brief Last known temperature driver health after init, temp_read_raw, or temp_read.
 * @return true if last init/read succeeded; false otherwise.
 */
bool temp_is_ok(void);

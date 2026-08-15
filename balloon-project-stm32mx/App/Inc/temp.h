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
 * MAX31865 uses IMU/LoRa bit-7 SPI R/W via spi_bus_read_reg8 / spi_bus_write_reg8.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Reference resistor on PCB (R21), ohms — for F4.2 resistance math. */
#define TEMP_RREF_OHM  4300.0f

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
 * @brief Last known temperature driver health after temp_init (F4.1).
 * @return true if last init succeeded; false otherwise.
 */
bool temp_is_ok(void);

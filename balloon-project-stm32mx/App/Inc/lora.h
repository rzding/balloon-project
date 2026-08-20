/**
 * @file lora.h
 * @brief RFM95W-915S2 (SX1276) LoRa driver (SPI1 + LoRa_CS + LoRa_RESET).
 *
 * F7.1: hardware reset via LoRa_RESET, VERSION register identity probe.
 * F7.2: LoRa modem configuration (frequency, SF/BW/CR, TX power).
 * F7.3: FIFO packet TX, DIO0 wait, sequence number.
 *
 * Locked init defaults (F7.2 — interim starting proposal, O5 team freeze pending):
 *   - 915.0 MHz US ISM, LoRa explicit header, SF8, BW125 kHz, CR4/5, CRC on
 *   - Private sync word 0x12 (not LoRaWAN public 0x34)
 *   - PA_BOOST +17 dBm, OCP on; chip left in LoRa standby (no TX in F7.2)
 *
 * SX1276 uses IMU/LoRa bit-7 SPI R/W via spi_bus_read_reg8 / spi_bus_write_reg8.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** SX1276 crystal frequency (Hz). */
#define LORA_FXOSC_HZ           32000000u

/** Locked carrier frequency (Hz). */
#define LORA_FREQ_HZ            915000000u

/** Locked private sync word (ground RX must match). */
#define LORA_SYNC_WORD          0x12u

/** Locked preamble length (symbols). */
#define LORA_PREAMBLE_LEN       8u

/**
 * @brief Convert carrier frequency to SX1276 Frf register value.
 *
 * Frf = (freq_hz * 2^19) / F_XOSC per SX1276 datasheet §5.5.2.
 *
 * @param freq_hz Carrier frequency in Hz.
 * @return 24-bit Frf value for RegFrfMsb/Mid/Lsb.
 */
static inline uint32_t lora_hz_to_frf(uint32_t freq_hz)
{
  return (uint32_t)(((uint64_t)freq_hz << 19) / (uint64_t)LORA_FXOSC_HZ);
}

/**
 * @brief Hardware-reset the RFM95W, probe VERSION, and configure LoRa modem.
 *
 * Pulses LoRa_RESET (active-low), waits for POR, reads RegVersion (0x42,
 * expect 0x12), then programs Sleep+LoRa → modem/PA → LoRa standby with
 * locked F7.2 defaults. Read-back verified on critical registers.
 * Updates error_flags on success or failure. No TX in F7.2.
 *
 * @return false on SPI failure, VERSION mismatch, or config failure; true on success.
 */
bool lora_init(void);

/**
 * @brief Last known LoRa driver health after lora_init.
 * @return true if last init succeeded; false otherwise.
 */
bool lora_is_ok(void);

/**
 * @brief Last VERSION register byte read during lora_init (bench/GDB helper).
 * @return RegVersion value (0 if never read successfully).
 */
uint8_t lora_get_version(void);

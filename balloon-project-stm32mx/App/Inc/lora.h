/**
 * @file lora.h
 * @brief RFM95W-915S2 (SX1276) LoRa driver (SPI1 + LoRa_CS + LoRa_RESET).
 *
 * F7.1: hardware reset via LoRa_RESET, VERSION register identity probe.
 * F7.2: LoRa modem configuration (frequency, SF/BW/CR, TX power).
 * F7.3: FIFO packet TX, DIO0 wait, sequence number.
 *
 * SX1276 uses IMU/LoRa bit-7 SPI R/W via spi_bus_read_reg8 / spi_bus_write_reg8.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Hardware-reset the RFM95W and probe SX1276 VERSION register.
 *
 * Pulses LoRa_RESET (active-low), waits for POR, reads RegVersion (0x42,
 * expect 0x12). Updates error_flags on success or failure. Leaves chip in
 * post-reset sleep (no OpMode/LoRa config in F7.1).
 *
 * @return false on SPI failure or VERSION mismatch; true on success.
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

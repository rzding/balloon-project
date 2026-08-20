/**
 * @file spi_bus.h
 * @brief Shared SPI1 bus owner — clock policy (F1.1), transfer (F1.2), register helpers (F1.3).
 *
 * SPI1 slaves and datasheet max SPI clocks (bring-up stays ~0.78 MHz until proven):
 *   - ICM-42688-P (IMU):     up to 24 MHz
 *   - MS5611-01BA03 (baro):  up to 20 MHz
 *   - MAX31865 (temp RTD):   <= 5 MHz
 *   - RFM95W (LoRa):         up to 10 MHz
 *   - microSD (SPI mode):    typically <= 25 MHz
 *   - ArduCAM:               SKU-dependent; consult datasheet
 *
 * Default bring-up: SPI_BAUDRATEPRESCALER_128 on 100 MHz APB2 -> ~781 kHz (roadmap §2.3).
 * Callers pass Cube CS macros (e.g. IMU_CS_GPIO_Port, IMU_CS_Pin from main.h).
 *
 * Register helpers use bit-7 R/W convention (read = reg|0x80, write = reg&0x7F) for
 * ICM-42688-P and SX1276/RFM95W. Other slaves use spi_bus_transfer directly.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

/** Default SPI1 bring-up prescaler (APB2 100 MHz / 128 ~ 0.781 MHz). */
#define SPI_BUS_DEFAULT_PRESCALER SPI_BAUDRATEPRESCALER_128

/** SPI Mode 0 (CPOL=0, CPHA=0) — bus default; used by IMU and baro. */
#define SPI_BUS_MODE0_POLARITY SPI_POLARITY_LOW
#define SPI_BUS_MODE0_PHASE    SPI_PHASE_1EDGE

/** SPI Mode 1 (CPOL=0, CPHA=1) — required by MAX31865 (datasheet excludes Mode 0). */
#define SPI_BUS_MODE1_POLARITY SPI_POLARITY_LOW
#define SPI_BUS_MODE1_PHASE    SPI_PHASE_2EDGE

/**
 * @brief Bind SPI1 handle and apply bring-up clock policy.
 * @param hspi CubeMX SPI1 handle (e.g. &hspi1).
 * @return false if @p hspi is null or HAL re-init fails.
 */
bool spi_bus_init(SPI_HandleTypeDef *hspi);

/**
 * @brief Change SPI baud-rate prescaler (for later per-device tuning).
 * @param baudrate_prescaler HAL SPI_BAUDRATEPRESCALER_* constant.
 * @return false if bus not initialized or HAL re-init fails.
 */
bool spi_bus_set_prescaler(uint32_t baudrate_prescaler);

/**
 * @brief Change SPI clock polarity/phase for a slave that needs a mode other
 *        than the SPI1 default (e.g. MAX31865, which requires Mode 1 or 3).
 * @param polarity HAL SPI_POLARITY_* constant.
 * @param phase    HAL SPI_PHASE_* constant.
 * @return false if bus not initialized or HAL re-init fails.
 */
bool spi_bus_set_mode(uint32_t polarity, uint32_t phase);

/**
 * @brief SPI transfer with chip-select ownership (one CS low at a time).
 *
 * Asserts CS active-low, performs the HAL transfer with a finite timeout, then
 * deasserts CS (high). CS is always restored high if it was asserted, including
 * on HAL timeout or error (then HAL_SPI_Abort is called).
 *
 * @param cs_port GPIO port for the slave CS (e.g. IMU_CS_GPIO_Port).
 * @param cs_pin  GPIO pin for the slave CS (e.g. IMU_CS_Pin).
 * @param tx      TX buffer; NULL for receive-only.
 * @param rx      RX buffer; NULL for transmit-only.
 * @param len     Byte count (must be > 0).
 * @param timeout_ms HAL timeout in milliseconds.
 * @return true on success; false on invalid args, bus busy, or transfer failure.
 */
bool spi_bus_transfer(GPIO_TypeDef *cs_port, uint16_t cs_pin,
                      const uint8_t *tx, uint8_t *rx,
                      uint16_t len, uint32_t timeout_ms);

/**
 * @brief Read one 8-bit register (IMU/LoRa bit-7 read convention).
 *
 * Sends (reg | 0x80) then a dummy byte; stores the second received byte in @p value.
 *
 * @param value Out pointer for register data; must not be NULL.
 * @return true on success; false on invalid args or transfer failure.
 */
bool spi_bus_read_reg8(GPIO_TypeDef *cs_port, uint16_t cs_pin,
                       uint8_t reg, uint8_t *value, uint32_t timeout_ms);

/**
 * @brief Write one 8-bit register (IMU/LoRa bit-7 write convention).
 *
 * Sends (reg & 0x7F) then @p value in one CS-framed transfer.
 *
 * @return true on success; false on invalid args or transfer failure.
 */
bool spi_bus_write_reg8(GPIO_TypeDef *cs_port, uint16_t cs_pin,
                        uint8_t reg, uint8_t value, uint32_t timeout_ms);

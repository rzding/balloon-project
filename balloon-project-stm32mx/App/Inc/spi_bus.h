/**
 * @file spi_bus.h
 * @brief Shared SPI1 bus owner — clock policy (F1.1) and CS-aware transfer (F1.2).
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
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

/** Default SPI1 bring-up prescaler (APB2 100 MHz / 128 ~ 0.781 MHz). */
#define SPI_BUS_DEFAULT_PRESCALER SPI_BAUDRATEPRESCALER_128

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

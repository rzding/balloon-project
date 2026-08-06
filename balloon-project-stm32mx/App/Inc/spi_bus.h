/**
 * @file spi_bus.h
 * @brief Shared SPI1 bus owner — clock policy (F1.1); transfer API in F1.2.
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

/**
 * @file imu.h
 * @brief ICM-42688-P IMU driver (SPI1 + IMU_CS).
 *
 * F2.1: WHO_AM_I identity probe.
 * F2.2: soft reset, SPI-only interface, FS/ODR configuration, LN power-on.
 * F2.3+: sample read and ongoing health updates.
 *
 * Locked init defaults (F2.2): accel ±16 g, gyro ±2000 dps, 100 Hz ODR.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Probe IMU identity and apply device configuration.
 *
 * Reads WHO_AM_I (0x75, expect 0x47), soft-resets, re-probes, then configures
 * SPI-only interface, ±16 g / ±2000 dps at 100 Hz ODR, and low-noise power.
 * Register writes are read-back verified. Updates error_flags on success or failure.
 *
 * @return false on SPI failure, ID mismatch, or configuration failure; true on success.
 */
bool imu_init(void);

/**
 * @brief Last known IMU health after init (or last init-time update).
 * @return true if IMU passed identity and configuration; false otherwise.
 */
bool imu_is_ok(void);

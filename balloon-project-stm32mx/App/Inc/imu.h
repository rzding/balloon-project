/**
 * @file imu.h
 * @brief ICM-42688-P IMU driver (SPI1 + IMU_CS).
 *
 * Phase F2.1: WHO_AM_I identity probe. Later phases add configuration,
 * sample read, and ongoing health updates.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Probe IMU identity via WHO_AM_I register.
 *
 * Reads WHO_AM_I (0x75) and compares to expected ICM-42688-P ID (0x47).
 * Updates error_flags and local health state on success or failure.
 *
 * @return false on SPI failure or ID mismatch; true on success.
 */
bool imu_init(void);

/**
 * @brief Last known IMU health after init (or last init-time update).
 * @return true if IMU passed identity check; false otherwise.
 */
bool imu_is_ok(void);

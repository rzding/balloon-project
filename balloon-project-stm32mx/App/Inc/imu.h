/**
 * @file imu.h
 * @brief ICM-42688-P IMU driver (SPI1 + IMU_CS).
 *
 * F2.1: WHO_AM_I identity probe.
 * F2.2: soft reset, SPI-only interface, FS/ODR configuration, LN power-on.
 * F2.3: polling sample read (accel + gyro raw LSB) and SI scale helpers.
 * F2.4: ongoing health via imu_read and imu_is_ok.
 *
 * Locked init defaults (F2.2): accel ±16 g, gyro ±2000 dps, 100 Hz ODR.
 * Scale helpers assume that FS (2048 LSB/g, 16.4 LSB/dps); host-testable.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Accel sensitivity at ±16 g (ACCEL_FS_SEL = 0). */
#define IMU_ACCEL_LSB_PER_G     2048.0f

/** Gyro sensitivity at ±2000 dps (GYRO_FS_SEL = 0). */
#define IMU_GYRO_LSB_PER_DPS    16.4f

/** Standard gravity in m/s² for accel SI conversion. */
#define IMU_GRAVITY_MPS2        9.80665f

/** Raw accel/gyro sample (16-bit two's complement LSB, big-endian from device). */
typedef struct
{
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t gx;
  int16_t gy;
  int16_t gz;
} imu_sample_t;

/**
 * @brief Combine big-endian register bytes into signed 16-bit sample.
 * @param hi MSB (register X1).
 * @param lo LSB (register X0).
 */
static inline int16_t imu_be_bytes_to_i16(uint8_t hi, uint8_t lo)
{
  return (int16_t)((uint16_t)hi << 8 | (uint16_t)lo);
}

/**
 * @brief Convert raw accel LSB (±16 g) to m/s².
 */
static inline float imu_accel_raw_to_mps2(int16_t raw)
{
  return ((float)raw / IMU_ACCEL_LSB_PER_G) * IMU_GRAVITY_MPS2;
}

/**
 * @brief Convert raw gyro LSB (±2000 dps) to degrees per second.
 */
static inline float imu_gyro_raw_to_dps(int16_t raw)
{
  return (float)raw / IMU_GYRO_LSB_PER_DPS;
}

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
 * @brief Poll accel and gyro XYZ via one SPI burst (polling; no INT1 in v1).
 *
 * Reads ACCEL_DATA_X1..GYRO_DATA_Z0 (12 bytes). Updates health on success or failure.
 *
 * @param out Out sample; must not be NULL.
 * @return false on NULL @p out or SPI failure; true on success.
 */
bool imu_read(imu_sample_t *out);

/**
 * @brief Last known IMU health after init or last imu_read.
 * @return true if last init/read succeeded; false otherwise.
 */
bool imu_is_ok(void);

/**
 * @file error_flags.h
 * @brief Subsystem health flags for fail-operational mission behavior.
 *
 * Polarity: ERR_FLAG_* bit set = subsystem fault; error_flags_*_ok() true = healthy.
 * Drivers call error_flags_set_*_ok(false) on init/comms failure; mission logic
 * continues even when flags are false.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** @name Subsystem error flag bits */
/**@{*/
#define ERR_FLAG_IMU  (1u << 0)
#define ERR_FLAG_BARO (1u << 1)
#define ERR_FLAG_TEMP (1u << 2)
#define ERR_FLAG_GPS  (1u << 3)
#define ERR_FLAG_SD   (1u << 4)
#define ERR_FLAG_LORA (1u << 5)
#define ERR_FLAG_CAM  (1u << 6)
#define ERR_FLAG_APRS (1u << 7)
/**@}*/

/** @brief Reset all flags to zero (all subsystems ok). */
void error_flags_init(void);

/** @brief Set one or more fault bits in @p mask. */
void error_flags_set(uint32_t mask);

/** @brief Clear one or more fault bits in @p mask. */
void error_flags_clear(uint32_t mask);

/**
 * @brief Test whether any fault bit in @p mask is set.
 * @return true if at least one bit in @p mask is set.
 */
bool error_flags_test(uint32_t mask);

/** @brief Return the raw fault flag word. */
uint32_t error_flags_get(void);

/** @name Per-subsystem ok accessors (healthy = true) */
/**@{*/
bool error_flags_imu_ok(void);
bool error_flags_baro_ok(void);
bool error_flags_temp_ok(void);
bool error_flags_gps_ok(void);
bool error_flags_sd_ok(void);
bool error_flags_lora_ok(void);
bool error_flags_cam_ok(void);
bool error_flags_aprs_ok(void);

void error_flags_set_imu_ok(bool ok);
void error_flags_set_baro_ok(bool ok);
void error_flags_set_temp_ok(bool ok);
void error_flags_set_gps_ok(bool ok);
void error_flags_set_sd_ok(bool ok);
void error_flags_set_lora_ok(bool ok);
void error_flags_set_cam_ok(bool ok);
void error_flags_set_aprs_ok(bool ok);
/**@}*/

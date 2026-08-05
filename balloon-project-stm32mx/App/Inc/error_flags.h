/**
 * @file error_flags.h
 * @brief Subsystem health flags for fail-operational mission behavior.
 *
 * Bit positions align with Phase F0.4 subsystem list. Drivers set/clear flags
 * on init and comms failure; mission logic continues even when flags are set.
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

/** @brief Reset all flags to zero. */
void error_flags_init(void);

/** @brief Set one or more bits in @p mask. */
void error_flags_set(uint32_t mask);

/** @brief Clear one or more bits in @p mask. */
void error_flags_clear(uint32_t mask);

/**
 * @brief Test whether any bit in @p mask is set.
 * @return true if at least one bit in @p mask is set.
 */
bool error_flags_test(uint32_t mask);

/** @brief Return the raw flag word. */
uint32_t error_flags_get(void);

/**
 * @file schedule.h
 * @brief Mission LoRa / camera / APRS rate schedulers (F8.2 / F10.3); pure logic, no HAL.
 *
 * Periods are integer milliseconds. Camera period 0 = disabled for that state.
 * F9 owns actual capture; schedule only signals due.
 * F10.3: APRS period is constant SCHEDULE_APRS_MS (BEACON-aligned).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mission.h"

/** LoRa period ASCENT / DESCENT / PAD / ARMED / BURST (ms) — ~0.5 Hz. */
#define SCHEDULE_LORA_ASCENT_MS     2000u
#define SCHEDULE_LORA_DESCENT_MS    2000u
#define SCHEDULE_LORA_PAD_MS        2000u
#define SCHEDULE_LORA_ARMED_MS      2000u
#define SCHEDULE_LORA_BURST_MS      2000u

/** LoRa period FLOAT (ms) — ~0.2 Hz. */
#define SCHEDULE_LORA_FLOAT_MS      5000u

/** LoRa period BEACON / LANDED (ms) — ~1/60 s. */
#define SCHEDULE_LORA_BEACON_MS     60000u
#define SCHEDULE_LORA_LANDED_MS     60000u

/** Camera period ASCENT / FLOAT (ms); 0 elsewhere = off. */
#define SCHEDULE_CAM_ASCENT_MS      30000u
#define SCHEDULE_CAM_FLOAT_MS       30000u

/** APRS period all states (ms) — F10.3: aligned with BEACON (60 s). */
#define SCHEDULE_APRS_MS            60000u

/**
 * @brief LoRa TX period for @p state (ms). Always > 0.
 */
uint32_t schedule_lora_period_ms(mission_state_t state);

/**
 * @brief Camera period for @p state (ms); 0 means camera off.
 */
uint32_t schedule_camera_period_ms(mission_state_t state);

/**
 * @brief APRS TX period (ms). Constant SCHEDULE_APRS_MS for all states.
 */
uint32_t schedule_aprs_period_ms(void);

/**
 * @brief Reset last-fire timestamps (treat as never fired).
 */
void schedule_init(void);

/**
 * @brief Poll due flags for current mission state.
 *
 * When due, advances last-fire to @p now_ms (not += period) to avoid burst
 * catch-up. State change does not force an immediate due.
 *
 * @param now_ms Current time (ms).
 * @param state Current mission state.
 * @param lora_due Out: true if LoRa/SD pack path should run; may be NULL.
 * @param cam_due Out: true if camera stub should run; may be NULL.
 * @param aprs_due Out: true if APRS TX should start; may be NULL.
 */
void schedule_poll(uint32_t now_ms, mission_state_t state,
                   bool *lora_due, bool *cam_due, bool *aprs_due);

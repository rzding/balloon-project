/**
 * @file mission.h
 * @brief Mission state machine (F8.1) — enum, thresholds, pure transition API.
 *
 * Wire values for packet_v1.mission_state (offset 1). No HAL — host-testable.
 * Schedulers (F8.2) live in schedule.h; packetizer fill (F8.3) is separate.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** @name Packet / ground wire values for mission_state */
/**@{*/
typedef enum
{
  MISSION_STATE_PAD     = 0,
  MISSION_STATE_ARMED   = 1,
  MISSION_STATE_ASCENT  = 2,
  MISSION_STATE_FLOAT   = 3,
  MISSION_STATE_BURST   = 4,
  MISSION_STATE_DESCENT = 5,
  MISSION_STATE_LANDED  = 6,
  MISSION_STATE_BEACON  = 7
} mission_state_t;
/**@}*/

/** Optional hold after healthy altitude source before PAD→ARMED (ms). */
#define MISSION_ARM_HOLD_MS             10000u

/** Sustained climb rate to enter ASCENT from ARMED (m/s). */
#define MISSION_ASCENT_RATE_MPS         1.5f

/** How long ascent rate must stay above threshold (ms). */
#define MISSION_ASCENT_SUSTAIN_MS       10000u

/** Minimum altitude for FLOAT (m, ISA / baro preferred). */
#define MISSION_FLOAT_ALT_M             18000.0f

/** |rate| below this counts as near-zero for FLOAT (m/s). */
#define MISSION_FLOAT_RATE_ABS_MPS      1.0f

/** Sustain near-zero rate at high alt before FLOAT (ms). */
#define MISSION_FLOAT_SUSTAIN_MS        30000u

/** Large negative rate → BURST (m/s); from Balloon Project.md. */
#define MISSION_BURST_RATE_MPS          (-50.0f)

/** Accel magnitude below this (g) counts toward freefall when IMU valid. */
#define MISSION_FREEFALL_ACCEL_G        0.3f

/** Freefall must persist this long (ms) to trigger BURST. */
#define MISSION_FREEFALL_SUSTAIN_MS     500u

/** |rate| below this for LANDED stability (m/s). */
#define MISSION_LANDED_RATE_ABS_MPS     0.5f

/** Altitude stable duration for DESCENT→LANDED (ms); roadmap §13. */
#define MISSION_LANDED_STABLE_MS        60000u

/** EMA coefficient for vertical rate (0..1); higher = smoother. */
#define MISSION_RATE_EMA_ALPHA          0.3f

/**
 * @brief Injected sample for one mission_update (no HAL).
 *
 * Prefer baro altitude when available; else GPS alt. Set altitude_source_ok when
 * baro is healthy or GPS altitude is valid (ARMED gate). IMU axes in g.
 */
typedef struct
{
  uint32_t time_ms;
  float alt_m;
  bool alt_valid;
  bool altitude_source_ok;
  float accel_g[3];
  bool imu_valid;
} mission_input_t;

/**
 * @brief Reset to PAD; clear latch, rate, and sustain timers.
 */
void mission_init(void);

/**
 * @brief Advance the state machine with one injected sample.
 *
 * Fail-soft: missing altitude holds the current state (except PAD arming which
 * requires altitude_source_ok). IMU failure disables freefall path only.
 *
 * @param in Non-NULL input sample.
 */
void mission_update(const mission_input_t *in);

/**
 * @brief Current mission state (wire value).
 */
mission_state_t mission_get_state(void);

/**
 * @brief True after BURST has been entered (never clears).
 */
bool mission_burst_latched(void);

/**
 * @brief Last filtered vertical rate (m/s); 0 if never computed.
 */
float mission_get_rate_mps(void);

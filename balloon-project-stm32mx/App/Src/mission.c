/**
 * @file mission.c
 * @brief Mission state machine transitions (F8.1); pure logic, no HAL.
 */

#include "mission.h"

#include <math.h>
#include <stddef.h>

static mission_state_t s_state;
static bool s_burst_latched;

static bool s_have_alt;
static float s_prev_alt_m;
static uint32_t s_prev_alt_ms;
static float s_rate_mps;
static float s_inst_rate_mps;
static bool s_have_rate;

static uint32_t s_arm_since_ms;
static bool s_arm_timing;

static uint32_t s_ascent_since_ms;
static bool s_ascent_timing;

static uint32_t s_float_since_ms;
static bool s_float_timing;

static uint32_t s_freefall_since_ms;
static bool s_freefall_timing;

static uint32_t s_landed_since_ms;
static bool s_landed_timing;

static float accel_mag_g(const mission_input_t *in)
{
  const float ax = in->accel_g[0];
  const float ay = in->accel_g[1];
  const float az = in->accel_g[2];
  return sqrtf(ax * ax + ay * ay + az * az);
}

static void update_rate(const mission_input_t *in)
{
  if (!in->alt_valid)
  {
    return;
  }

  if (!s_have_alt)
  {
    s_prev_alt_m = in->alt_m;
    s_prev_alt_ms = in->time_ms;
    s_have_alt = true;
    return;
  }

  if (in->time_ms == s_prev_alt_ms)
  {
    return;
  }

  {
    const float dt_s = (float)(in->time_ms - s_prev_alt_ms) / 1000.0f;
    float inst;

    if (dt_s <= 0.0f || dt_s > 30.0f)
    {
      /* Stale or time went backwards — resync without using rate. */
      s_prev_alt_m = in->alt_m;
      s_prev_alt_ms = in->time_ms;
      return;
    }

    inst = (in->alt_m - s_prev_alt_m) / dt_s;
    s_inst_rate_mps = inst;
    if (!s_have_rate)
    {
      s_rate_mps = inst;
      s_have_rate = true;
    }
    else
    {
      s_rate_mps = MISSION_RATE_EMA_ALPHA * s_rate_mps +
                   (1.0f - MISSION_RATE_EMA_ALPHA) * inst;
    }

    s_prev_alt_m = in->alt_m;
    s_prev_alt_ms = in->time_ms;
  }
}

static bool burst_criteria(const mission_input_t *in)
{
  /* Instantaneous rate for burst (EMA lags a step-drop below −50 m/s). */
  if (s_have_rate && s_inst_rate_mps <= MISSION_BURST_RATE_MPS)
  {
    return true;
  }
  if (s_have_rate && s_rate_mps <= MISSION_BURST_RATE_MPS)
  {
    return true;
  }

  if (!in->imu_valid)
  {
    s_freefall_timing = false;
    return false;
  }

  if (accel_mag_g(in) < MISSION_FREEFALL_ACCEL_G)
  {
    if (!s_freefall_timing)
    {
      s_freefall_since_ms = in->time_ms;
      s_freefall_timing = true;
    }
    else if ((in->time_ms - s_freefall_since_ms) >= MISSION_FREEFALL_SUSTAIN_MS)
    {
      return true;
    }
  }
  else
  {
    s_freefall_timing = false;
  }

  return false;
}

static void enter_burst(void)
{
  s_state = MISSION_STATE_BURST;
  s_burst_latched = true;
  s_ascent_timing = false;
  s_float_timing = false;
  s_freefall_timing = false;
}

void mission_init(void)
{
  s_state = MISSION_STATE_PAD;
  s_burst_latched = false;
  s_have_alt = false;
  s_prev_alt_m = 0.0f;
  s_prev_alt_ms = 0u;
  s_rate_mps = 0.0f;
  s_inst_rate_mps = 0.0f;
  s_have_rate = false;
  s_arm_timing = false;
  s_ascent_timing = false;
  s_float_timing = false;
  s_freefall_timing = false;
  s_landed_timing = false;
}

void mission_update(const mission_input_t *in)
{
  if (in == NULL)
  {
    return;
  }

  update_rate(in);

  switch (s_state)
  {
    case MISSION_STATE_PAD:
      if (in->altitude_source_ok)
      {
        if (!s_arm_timing)
        {
          s_arm_since_ms = in->time_ms;
          s_arm_timing = true;
        }
        else if ((in->time_ms - s_arm_since_ms) >= MISSION_ARM_HOLD_MS)
        {
          s_state = MISSION_STATE_ARMED;
          s_arm_timing = false;
        }
      }
      else
      {
        s_arm_timing = false;
      }
      break;

    case MISSION_STATE_ARMED:
      if (burst_criteria(in))
      {
        enter_burst();
        break;
      }
      if (s_have_rate && s_rate_mps >= MISSION_ASCENT_RATE_MPS)
      {
        if (!s_ascent_timing)
        {
          s_ascent_since_ms = in->time_ms;
          s_ascent_timing = true;
        }
        else if ((in->time_ms - s_ascent_since_ms) >= MISSION_ASCENT_SUSTAIN_MS)
        {
          s_state = MISSION_STATE_ASCENT;
          s_ascent_timing = false;
        }
      }
      else
      {
        s_ascent_timing = false;
      }
      break;

    case MISSION_STATE_ASCENT:
      if (burst_criteria(in))
      {
        enter_burst();
        break;
      }
      if (in->alt_valid && in->alt_m >= MISSION_FLOAT_ALT_M && s_have_rate &&
          fabsf(s_rate_mps) <= MISSION_FLOAT_RATE_ABS_MPS)
      {
        if (!s_float_timing)
        {
          s_float_since_ms = in->time_ms;
          s_float_timing = true;
        }
        else if ((in->time_ms - s_float_since_ms) >= MISSION_FLOAT_SUSTAIN_MS)
        {
          s_state = MISSION_STATE_FLOAT;
          s_float_timing = false;
        }
      }
      else
      {
        s_float_timing = false;
      }
      break;

    case MISSION_STATE_FLOAT:
      if (burst_criteria(in))
      {
        enter_burst();
      }
      break;

    case MISSION_STATE_BURST:
      /* One-tick dwell then DESCENT; latch prevents return to ASCENT/FLOAT. */
      s_state = MISSION_STATE_DESCENT;
      s_landed_timing = false;
      break;

    case MISSION_STATE_DESCENT:
      /* Latch: never return to ASCENT/FLOAT even if rate slows. */
      if (s_have_rate && fabsf(s_rate_mps) <= MISSION_LANDED_RATE_ABS_MPS &&
          in->alt_valid)
      {
        if (!s_landed_timing)
        {
          s_landed_since_ms = in->time_ms;
          s_landed_timing = true;
        }
        else if ((in->time_ms - s_landed_since_ms) >= MISSION_LANDED_STABLE_MS)
        {
          s_state = MISSION_STATE_LANDED;
          s_landed_timing = false;
        }
      }
      else
      {
        s_landed_timing = false;
      }
      break;

    case MISSION_STATE_LANDED:
      s_state = MISSION_STATE_BEACON;
      break;

    case MISSION_STATE_BEACON:
    default:
      break;
  }
}

mission_state_t mission_get_state(void)
{
  return s_state;
}

bool mission_burst_latched(void)
{
  return s_burst_latched;
}

float mission_get_rate_mps(void)
{
  return s_have_rate ? s_rate_mps : 0.0f;
}

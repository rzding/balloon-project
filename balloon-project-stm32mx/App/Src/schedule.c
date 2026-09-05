/**
 * @file schedule.c
 * @brief Per-state LoRa / camera interval scheduler (F8.2).
 */

#include "schedule.h"

#include <stddef.h>

static uint32_t s_last_lora_ms;
static uint32_t s_last_cam_ms;
static bool s_lora_armed;
static bool s_cam_armed;

uint32_t schedule_lora_period_ms(mission_state_t state)
{
  switch (state)
  {
    case MISSION_STATE_FLOAT:
      return SCHEDULE_LORA_FLOAT_MS;
    case MISSION_STATE_BEACON:
      return SCHEDULE_LORA_BEACON_MS;
    case MISSION_STATE_LANDED:
      return SCHEDULE_LORA_LANDED_MS;
    case MISSION_STATE_ASCENT:
      return SCHEDULE_LORA_ASCENT_MS;
    case MISSION_STATE_DESCENT:
      return SCHEDULE_LORA_DESCENT_MS;
    case MISSION_STATE_PAD:
      return SCHEDULE_LORA_PAD_MS;
    case MISSION_STATE_ARMED:
      return SCHEDULE_LORA_ARMED_MS;
    case MISSION_STATE_BURST:
      return SCHEDULE_LORA_BURST_MS;
    default:
      return SCHEDULE_LORA_ASCENT_MS;
  }
}

uint32_t schedule_camera_period_ms(mission_state_t state)
{
  switch (state)
  {
    case MISSION_STATE_ASCENT:
      return SCHEDULE_CAM_ASCENT_MS;
    case MISSION_STATE_FLOAT:
      return SCHEDULE_CAM_FLOAT_MS;
    default:
      return 0u;
  }
}

void schedule_init(void)
{
  s_last_lora_ms = 0u;
  s_last_cam_ms = 0u;
  s_lora_armed = false;
  s_cam_armed = false;
}

void schedule_poll(uint32_t now_ms, mission_state_t state,
                   bool *lora_due, bool *cam_due)
{
  const uint32_t lora_period = schedule_lora_period_ms(state);
  const uint32_t cam_period = schedule_camera_period_ms(state);
  bool lora = false;
  bool cam = false;

  if (!s_lora_armed)
  {
    /* First poll: arm without immediate TX (wait one full period). */
    s_last_lora_ms = now_ms;
    s_lora_armed = true;
  }
  else if ((now_ms - s_last_lora_ms) >= lora_period)
  {
    lora = true;
    s_last_lora_ms = now_ms;
  }

  if (cam_period == 0u)
  {
    /* Camera off: do not accumulate overdue; re-arm baseline for when enabled. */
    s_last_cam_ms = now_ms;
    s_cam_armed = true;
  }
  else if (!s_cam_armed)
  {
    s_last_cam_ms = now_ms;
    s_cam_armed = true;
  }
  else if ((now_ms - s_last_cam_ms) >= cam_period)
  {
    cam = true;
    s_last_cam_ms = now_ms;
  }

  if (lora_due != NULL)
  {
    *lora_due = lora;
  }
  if (cam_due != NULL)
  {
    *cam_due = cam;
  }
}

/**
 * @file error_flags.c
 * @brief Subsystem health flag storage and accessors.
 */

#include "error_flags.h"

static uint32_t s_flags;

static void set_ok(uint32_t flag, bool ok)
{
  if (ok)
  {
    error_flags_clear(flag);
  }
  else
  {
    error_flags_set(flag);
  }
}

static bool get_ok(uint32_t flag)
{
  return !error_flags_test(flag);
}

void error_flags_init(void)
{
  s_flags = 0u;
}

void error_flags_set(uint32_t mask)
{
  s_flags |= mask;
}

void error_flags_clear(uint32_t mask)
{
  s_flags &= ~mask;
}

bool error_flags_test(uint32_t mask)
{
  return (s_flags & mask) != 0u;
}

uint32_t error_flags_get(void)
{
  return s_flags;
}

bool error_flags_imu_ok(void)
{
  return get_ok(ERR_FLAG_IMU);
}

bool error_flags_baro_ok(void)
{
  return get_ok(ERR_FLAG_BARO);
}

bool error_flags_temp_ok(void)
{
  return get_ok(ERR_FLAG_TEMP);
}

bool error_flags_gps_ok(void)
{
  return get_ok(ERR_FLAG_GPS);
}

bool error_flags_sd_ok(void)
{
  return get_ok(ERR_FLAG_SD);
}

bool error_flags_lora_ok(void)
{
  return get_ok(ERR_FLAG_LORA);
}

bool error_flags_cam_ok(void)
{
  return get_ok(ERR_FLAG_CAM);
}

bool error_flags_aprs_ok(void)
{
  return get_ok(ERR_FLAG_APRS);
}

void error_flags_set_imu_ok(bool ok)
{
  set_ok(ERR_FLAG_IMU, ok);
}

void error_flags_set_baro_ok(bool ok)
{
  set_ok(ERR_FLAG_BARO, ok);
}

void error_flags_set_temp_ok(bool ok)
{
  set_ok(ERR_FLAG_TEMP, ok);
}

void error_flags_set_gps_ok(bool ok)
{
  set_ok(ERR_FLAG_GPS, ok);
}

void error_flags_set_sd_ok(bool ok)
{
  set_ok(ERR_FLAG_SD, ok);
}

void error_flags_set_lora_ok(bool ok)
{
  set_ok(ERR_FLAG_LORA, ok);
}

void error_flags_set_cam_ok(bool ok)
{
  set_ok(ERR_FLAG_CAM, ok);
}

void error_flags_set_aprs_ok(bool ok)
{
  set_ok(ERR_FLAG_APRS, ok);
}

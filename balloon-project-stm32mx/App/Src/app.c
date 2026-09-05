/**
 * @file app.c
 * @brief Application entry point implementation.
 */

#include "app.h"

#include "aprs.h"
#include "baro.h"
#include "error_flags.h"
#include "gps.h"
#include "imu.h"
#include "lora.h"
#include "main.h"
#include "mission.h"
#include "packet.h"
#include "packetizer.h"
#include "schedule.h"
#include "sdlog.h"
#include "temp.h"

/** Altitude refresh period for mission SM (ms); IMU polled every superloop. */
#define APP_MISSION_ALT_PERIOD_MS  1000u

/*
 * Telemetry / schedule debug counters.
 *
 * Deliberately NOT static: external linkage lets GDB resolve them from any
 * stop location. volatile keeps them observable if optimisation rises.
 */
static uint32_t s_mission_alt_next_ms;
static float s_mission_alt_m;
static bool s_mission_alt_valid;
static bool s_mission_alt_source_ok;

volatile uint32_t g_beacon_attempts;
volatile uint32_t g_beacon_ok;
volatile uint32_t g_beacon_fail;
volatile uint32_t g_cam_due_count;
packet_v1_t g_beacon_fields;
uint8_t g_beacon_wire[PACKET_V1_LEN];

bool app_init(void)
{
  error_flags_init();
  (void)imu_init();  /* fail-soft: false does not abort app_init */
  (void)baro_init(); /* fail-soft: false does not abort app_init */
  (void)temp_init(); /* fail-soft: false does not abort app_init */
  (void)gps_init();  /* fail-soft: false does not abort app_init */
  (void)lora_init(); /* fail-soft: false does not abort app_init */
  (void)aprs_init(); /* fail-soft: false does not abort app_init */
  (void)sdlog_init();  /* fail-soft: false does not abort app_init */
  mission_init();
  schedule_init();
  s_mission_alt_m = 0.0f;
  s_mission_alt_valid = false;
  s_mission_alt_source_ok = false;
  g_cam_due_count = 0u;
  return true;
}

/** Refresh cached altitude for the mission SM (baro preferred, else GPS). */
static void app_mission_refresh_alt(void)
{
  baro_sample_t baro;
  gps_sample_t gps;
  bool baro_ok = false;
  bool gps_alt_ok = false;

  s_mission_alt_valid = false;
  s_mission_alt_source_ok = false;

  if (baro_is_ok() && baro_read(&baro))
  {
    s_mission_alt_m = baro.alt_m;
    s_mission_alt_valid = true;
    baro_ok = true;
  }

  if (gps_get_sample(&gps) && gps.alt_valid && gps.alt_m > 0)
  {
    gps_alt_ok = true;
    if (!s_mission_alt_valid)
    {
      s_mission_alt_m = (float)gps.alt_m;
      s_mission_alt_valid = true;
    }
  }

  s_mission_alt_source_ok = baro_ok || gps_alt_ok;
}

/**
 * @brief Feed mission_update: IMU every call; altitude on APP_MISSION_ALT_PERIOD_MS.
 */
static void app_mission_tick(void)
{
  mission_input_t in;
  imu_sample_t imu;
  const uint32_t now = HAL_GetTick();
  unsigned i;

  in.time_ms = now;
  in.alt_m = s_mission_alt_m;
  in.alt_valid = s_mission_alt_valid;
  in.altitude_source_ok = s_mission_alt_source_ok;
  in.imu_valid = false;
  for (i = 0u; i < 3u; i++)
  {
    in.accel_g[i] = 0.0f;
  }

  if ((now - s_mission_alt_next_ms) >= APP_MISSION_ALT_PERIOD_MS)
  {
    s_mission_alt_next_ms = now;
    app_mission_refresh_alt();
    in.alt_m = s_mission_alt_m;
    in.alt_valid = s_mission_alt_valid;
    in.altitude_source_ok = s_mission_alt_source_ok;
  }

  if (imu_is_ok() && imu_read(&imu))
  {
    in.accel_g[0] = (float)imu.ax / IMU_ACCEL_LSB_PER_G;
    in.accel_g[1] = (float)imu.ay / IMU_ACCEL_LSB_PER_G;
    in.accel_g[2] = (float)imu.az / IMU_ACCEL_LSB_PER_G;
    in.imu_valid = true;
  }

  mission_update(&in);
}

/* Sample sensors into packetizer; unhealthy leave zeros / N/A via fill defaults. */
static void app_beacon_build(void)
{
  packetizer_sample_t sample;
  baro_sample_t baro;
  temp_sample_t temp;
  gps_sample_t gps;

  sample.mission_state = (uint8_t)mission_get_state();
  sample.seq = lora_get_seq();
  sample.time_ms = HAL_GetTick();
  sample.error_flags = error_flags_get();
  sample.baro_valid = false;
  sample.baro_alt_m = 0.0f;
  sample.baro_temp_centi_c = 0;
  sample.temp_valid = false;
  sample.temp_centi_c = 0;
  sample.gps_sample_valid = false;
  sample.sats = 0u;
  sample.lat_lon_valid = false;
  sample.lat_e7 = 0;
  sample.lon_e7 = 0;
  sample.gps_alt_valid = false;
  sample.gps_alt_m = 0u;

  if (baro_is_ok() && baro_read(&baro))
  {
    sample.baro_valid = true;
    sample.baro_alt_m = baro.alt_m;
    sample.baro_temp_centi_c = (int16_t)baro.temp_centi_c;
  }

  if (temp_is_ok() && temp_read(&temp))
  {
    sample.temp_valid = true;
    sample.temp_centi_c = temp.temp_centi_c;
  }

  if (gps_get_sample(&gps))
  {
    sample.gps_sample_valid = true;
    sample.sats = gps.sats;
    sample.lat_lon_valid = gps.lat_lon_valid;
    sample.lat_e7 = gps.lat_e7;
    sample.lon_e7 = gps.lon_e7;
    if (gps.alt_valid && gps.alt_m > 0)
    {
      sample.gps_alt_valid = true;
      sample.gps_alt_m = (uint16_t)gps.alt_m;
    }
  }

  packetizer_fill(&sample, &g_beacon_fields);
}

/**
 * @brief F9.3 hook — capture not implemented until ArduCAM SKU (F9).
 * LoRa must never wait on this path.
 */
static void app_camera_on_due(void)
{
  g_cam_due_count++;
  /* F9.3: camera_capture_to_sd(...); yield SPI between chunks. */
}

/* F8.2: LoRa/SD on schedule_poll lora_due; camera stub on cam_due. */
static void app_schedule_tick(void)
{
  bool lora_due = false;
  bool cam_due = false;
  const uint32_t now = HAL_GetTick();

  schedule_poll(now, mission_get_state(), &lora_due, &cam_due);

  if (cam_due)
  {
    app_camera_on_due();
  }

  if (!lora_due)
  {
    return;
  }

  app_beacon_build();

  /* F6: log every LoRa-due tick even if radio is dead */
  (void)sdlog_write_sample(
      g_beacon_fields.time_ms,
      (float)g_beacon_fields.temp_c_x100 / 100.0f,
      (float)g_beacon_fields.baro_alt_m);

  if (!lora_is_ok())
  {
    return;
  }

  packetizer_pack(&g_beacon_fields, g_beacon_wire);
  g_beacon_attempts++;
  if (lora_tx(g_beacon_wire, PACKET_V1_LEN))
  {
    g_beacon_ok++;
  }
  else
  {
    g_beacon_fail++;
  }
}

void app_run(void)
{
  /* Subsystem faults must not stop the superloop; mission tick runs regardless. */
  (void)gps_poll();
  app_mission_tick();
  app_schedule_tick();
}

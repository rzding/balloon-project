/**
 * @file app.c
 * @brief Application entry point implementation.
 */

#include "app.h"

#include "baro.h"
#include "error_flags.h"
#include "gps.h"
#include "imu.h"
#include "lora.h"
#include "main.h"
#include "mission.h"
#include "packet.h"
#include "sdlog.h"
#include "temp.h"

/** Beacon transmit period (ms). F8.2 will replace with per-state rates. */
#define APP_BEACON_PERIOD_MS       5000u

/** Altitude refresh period for mission SM (ms); IMU polled every superloop. */
#define APP_MISSION_ALT_PERIOD_MS  1000u

/* Bring-up telemetry beacon (F7/F8.1).
 *
 * Deliberately NOT static: file-local symbols can only be resolved by bare
 * name while halted inside this translation unit, which makes them awkward to
 * watch in the debugger. External linkage lets "g_beacon_ok" resolve from any
 * stop location. volatile on the counters keeps them observable even if the
 * optimisation level is raised later (nothing in the firmware reads them).
 */
static uint32_t s_beacon_next_ms;
static uint32_t s_mission_alt_next_ms;
static float s_mission_alt_m;
static bool s_mission_alt_valid;
static bool s_mission_alt_source_ok;

volatile uint32_t g_beacon_attempts;
volatile uint32_t g_beacon_ok;
volatile uint32_t g_beacon_fail;
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
  (void)sdlog_init();  /* fail-soft: false does not abort app_init */
  mission_init();
  s_mission_alt_m = 0.0f;
  s_mission_alt_valid = false;
  s_mission_alt_source_ok = false;
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

/* Fill g_beacon_fields from whatever sensors are healthy right now.
 * Unhealthy or unread sensors leave their fields at zero / not-available;
 * a beacon is still sent so the link itself can be tested with no fix. */
static void app_beacon_build(void)
{
  baro_sample_t baro;
  temp_sample_t temp;
  gps_sample_t gps;

  g_beacon_fields.version = PACKET_V1_VERSION;
  g_beacon_fields.mission_state = (uint8_t)mission_get_state();
  g_beacon_fields.seq = lora_get_seq();
  g_beacon_fields.time_ms = HAL_GetTick();
  g_beacon_fields.lat_e7 = 0;
  g_beacon_fields.lon_e7 = 0;
  g_beacon_fields.gps_alt_m = 0u;
  g_beacon_fields.baro_alt_m = 0;
  g_beacon_fields.temp_c_x100 = 0;
  g_beacon_fields.batt = PACKET_V1_BATT_NA;
  g_beacon_fields.sats = 0u;

  /* Packet v1 flags: bit set = healthy (OK polarity). See packet.h / ground/README.md. */
  g_beacon_fields.flags = (uint8_t)(~error_flags_get() & 0xFFu);

  if (baro_is_ok() && baro_read(&baro))
  {
    g_beacon_fields.baro_alt_m = (int16_t)baro.alt_m;
    g_beacon_fields.temp_c_x100 = (int16_t)baro.temp_centi_c;
  }

  /* MAX31865 is the better temperature source; let it override the baro die temp. */
  if (temp_is_ok() && temp_read(&temp))
  {
    g_beacon_fields.temp_c_x100 = temp.temp_centi_c;
  }

  if (gps_get_sample(&gps))
  {
    g_beacon_fields.sats = gps.sats;

    if (gps.lat_lon_valid)
    {
      g_beacon_fields.lat_e7 = gps.lat_e7;
      g_beacon_fields.lon_e7 = gps.lon_e7;
    }

    if (gps.alt_valid && gps.alt_m > 0)
    {
      g_beacon_fields.gps_alt_m = (uint16_t)gps.alt_m;
    }
  }
}

/* Transmit one packet v1 beacon every APP_BEACON_PERIOD_MS. */
static void app_beacon_tick(void)
{
  const uint32_t now = HAL_GetTick();

  if ((now - s_beacon_next_ms) >= APP_BEACON_PERIOD_MS)
  {
    s_beacon_next_ms = now;
    app_beacon_build();

    /* F6: log every beacon tick even if radio is dead */
    (void)sdlog_write_sample(
        g_beacon_fields.time_ms,
        (float)g_beacon_fields.temp_c_x100 / 100.0f,
        (float)g_beacon_fields.baro_alt_m);

    if (!lora_is_ok()) return;

    packet_v1_pack(&g_beacon_fields, g_beacon_wire);
    g_beacon_attempts++;
    if (lora_tx(g_beacon_wire, PACKET_V1_LEN)) g_beacon_ok++;
    else                                        g_beacon_fail++;
  }
}

void app_run(void)
{
  /* Subsystem faults must not stop the superloop; mission tick runs regardless. */
  (void)gps_poll();
  app_mission_tick();
  app_beacon_tick();
}

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
#include "packet.h"
#include "temp.h"

/** Beacon transmit period (ms). */
#define APP_BEACON_PERIOD_MS  5000u

/** Mission state reported in packet v1 while in bring-up. */
#define APP_MISSION_STATE_BENCH  0x00u

/* Bench telemetry beacon (F7.3 bring-up).
 *
 * Deliberately NOT static: file-local symbols can only be resolved by bare
 * name while halted inside this translation unit, which makes them awkward to
 * watch in the debugger. External linkage lets "g_beacon_ok" resolve from any
 * stop location. volatile on the counters keeps them observable even if the
 * optimisation level is raised later (nothing in the firmware reads them).
 */
static uint32_t s_beacon_next_ms;
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
  return true;
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
  g_beacon_fields.mission_state = APP_MISSION_STATE_BENCH;
  g_beacon_fields.seq = lora_get_seq();
  g_beacon_fields.time_ms = HAL_GetTick();
  g_beacon_fields.lat_e7 = 0;
  g_beacon_fields.lon_e7 = 0;
  g_beacon_fields.gps_alt_m = 0u;
  g_beacon_fields.baro_alt_m = 0;
  g_beacon_fields.temp_c_x100 = 0;
  g_beacon_fields.batt = PACKET_V1_BATT_NA;
  g_beacon_fields.sats = 0u;

  /* Low byte of the health bitfield: bit set = that subsystem is OK. */
  g_beacon_fields.flags = (uint8_t)(error_flags_get() & 0xFFu);

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

    if (!lora_is_ok())
    {
      return; /* radio never came up; nothing to transmit into */
    }

    app_beacon_build();
    packet_v1_pack(&g_beacon_fields, g_beacon_wire);

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
}

void app_run(void)
{
  /* Subsystem faults must not stop the superloop; F8+ mission tick runs regardless. */
  (void)gps_poll();
  (void)error_flags_get();
  app_beacon_tick();
}

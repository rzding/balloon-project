/**
 * @file app.c
 * @brief Application entry point implementation.
 */

#include "app.h"

#include "baro.h"
#include "camera.h"
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
#include "telem_img.h"
#include "telem_imu.h"
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

/* IMU telemetry + camera image downlink (F9.2). Non-static: GDB-visible. */
volatile uint8_t g_cam_capture_request; /* set 1 (GDB bench or cam_due) to grab+send a frame */
volatile uint8_t g_cam_verify_request;  /* set 1 to capture + validate WITHOUT the slow downlink */
volatile uint8_t g_cam_auto_downlink = 1u; /* 1 = auto capture+downlink every APP_IMG_AUTO_PERIOD_MS (no debugger) */
volatile uint32_t g_imu_beacons;        /* IMU packets transmitted */
volatile uint32_t g_cam_img_sends;      /* image headers transmitted */
volatile uint32_t g_cam_chunks_sent;    /* image data chunks transmitted */
volatile uint32_t g_cam_verify_count;   /* capture-verify runs completed */

#define APP_IMG_BUF_MAX   24576u              /* RAM cap for one JPEG */
#define APP_IMG_RES       CAMERA_RES_160x120  /* small enough for LoRa */
#define APP_IMG_CHUNK_MS  250u                /* pace between image chunks */
#define APP_IMG_AUTO_PERIOD_MS  12000u        /* auto capture+downlink cadence (photos with no debugger) */

static uint8_t s_img_buf[APP_IMG_BUF_MAX];
static uint32_t s_img_len;
static uint16_t s_img_total_chunks;
static uint16_t s_img_chunk_idx;
static uint8_t s_img_id;
static bool s_img_active;
static uint32_t s_img_next_ms;
static uint32_t s_img_auto_next_ms;
static uint16_t s_imu_seq;

bool app_init(void)
{
  error_flags_init();
  (void)imu_init();  /* fail-soft: false does not abort app_init */
  (void)baro_init(); /* fail-soft: false does not abort app_init */
  (void)temp_init(); /* fail-soft: false does not abort app_init */
  (void)gps_init();  /* fail-soft: false does not abort app_init */
  (void)lora_init(); /* fail-soft: false does not abort app_init */
  (void)sdlog_init();  /* fail-soft: false does not abort app_init */
  (void)camera_init(); /* fail-soft: false does not abort app_init */
  mission_init();
  schedule_init();
  s_mission_alt_m = 0.0f;
  s_mission_alt_valid = false;
  s_mission_alt_source_ok = false;
  g_cam_due_count = 0u;

  /* One capture-verify shortly after boot so the JPEG size + SOI/EOI are
     visible with no GDB poking. Runs on the first app_run (off the slow
     downlink path). Re-trigger any time by setting g_cam_verify_request = 1. */
  if (camera_is_ok())
  {
    g_cam_verify_request = 1u;
  }
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
  /* F9.2/F9.3: request a capture; app_image_tick does the work off this path
     so a slow image downlink never blocks scheduling here. */
  g_cam_capture_request = 1u;
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

  /* IMU telemetry rides alongside each beacon as its own packet (type 0x02). */
  if (imu_is_ok())
  {
    imu_sample_t imu;

    if (imu_read(&imu))
    {
      uint8_t iw[TELEM_IMU_LEN];

      telem_imu_pack(iw, (uint8_t)mission_get_state(), s_imu_seq, HAL_GetTick(), &imu);
      s_imu_seq++;
      if (lora_tx(iw, TELEM_IMU_LEN))
      {
        g_imu_beacons++;
      }
    }
  }
}

/*
 * Capture-verify (F9.1b). Grab one JPEG into s_img_buf so camera_capture records
 * the FIFO length and SOI/EOI validity globals, WITHOUT the slow chunk downlink.
 * Guarded off an active downlink (they share s_img_buf). Triggered by
 * g_cam_verify_request: set once at boot, and settable from GDB on the bench.
 * Read after it runs: g_cam_last_fifo_len (bytes), g_cam_soi_ok, g_cam_eoi_ok.
 */
static void app_camera_verify(void)
{
  uint32_t len = 0u;

  if (g_cam_verify_request == 0u || s_img_active)
  {
    return;
  }
  g_cam_verify_request = 0u;

  if (!camera_is_ok())
  {
    return;
  }
  if (!camera_jpeg_ready() && !camera_configure_jpeg(APP_IMG_RES))
  {
    return;
  }

  (void)camera_capture(s_img_buf, APP_IMG_BUF_MAX, &len);
  g_cam_verify_count++;
}

/*
 * F9.2 image downlink. On a capture request (GDB sets g_cam_capture_request, or
 * a scheduled cam_due does), grab a JPEG into s_img_buf and stream it as one
 * header packet plus N data chunks, one chunk per APP_IMG_CHUNK_MS so beacons
 * and mission ticks keep running between chunks. LoRa is slow: a 160x120 frame
 * takes ~10-20 s. When idle this costs one flag read per superloop.
 */
static void app_image_tick(void)
{
  const uint32_t now = HAL_GetTick();

  if (!s_img_active)
  {
    /* Auto-downlink: grab and send a frame on a timer so photos flow with no
       debugger poke. Independent of mission-state cam_due. Set
       g_cam_auto_downlink = 0 to stop it. */
    if ((g_cam_auto_downlink != 0u) && camera_is_ok() && lora_is_ok() &&
        ((now - s_img_auto_next_ms) >= APP_IMG_AUTO_PERIOD_MS))
    {
      s_img_auto_next_ms = now;
      g_cam_capture_request = 1u;
    }

    if (g_cam_capture_request == 0u)
    {
      return;
    }
    g_cam_capture_request = 0u;

    if (!camera_is_ok())
    {
      return;
    }
    if (!camera_jpeg_ready() && !camera_configure_jpeg(APP_IMG_RES))
    {
      return;
    }
    if (!camera_capture(s_img_buf, APP_IMG_BUF_MAX, &s_img_len) || s_img_len == 0u)
    {
      return;
    }

    s_img_id++;
    s_img_chunk_idx = 0u;
    s_img_total_chunks =
        (uint16_t)((s_img_len + TELEM_IMG_CHUNK_DATA - 1u) / TELEM_IMG_CHUNK_DATA);

    if (lora_is_ok())
    {
      uint8_t hdr[TELEM_IMG_HDR_LEN];

      telem_img_pack_header(hdr, s_img_id, s_img_len, s_img_total_chunks,
                            camera_res_width(camera_get_res()),
                            camera_res_height(camera_get_res()),
                            (uint16_t)TELEM_IMG_CHUNK_DATA);
      (void)lora_tx(hdr, TELEM_IMG_HDR_LEN);
      g_cam_img_sends++;
    }

    s_img_active = true;
    s_img_next_ms = now;
    return;
  }

  /* Active: one chunk per APP_IMG_CHUNK_MS. */
  if ((now - s_img_next_ms) < APP_IMG_CHUNK_MS)
  {
    return;
  }
  s_img_next_ms = now;

  if (!lora_is_ok())
  {
    s_img_active = false;
    return;
  }

  {
    const uint32_t off = (uint32_t)s_img_chunk_idx * TELEM_IMG_CHUNK_DATA;
    const uint32_t remain = s_img_len - off;
    const uint8_t dlen = (remain > TELEM_IMG_CHUNK_DATA)
                             ? (uint8_t)TELEM_IMG_CHUNK_DATA
                             : (uint8_t)remain;
    uint8_t pkt[TELEM_IMG_CHUNK_MAX];
    const uint8_t plen =
        telem_img_pack_chunk(pkt, s_img_id, s_img_chunk_idx, &s_img_buf[off], dlen);

    if (plen > 0u && lora_tx(pkt, plen))
    {
      g_cam_chunks_sent++;
    }

    s_img_chunk_idx++;
    if (s_img_chunk_idx >= s_img_total_chunks)
    {
      s_img_active = false;
    }
  }
}

void app_run(void)
{
  /* Subsystem faults must not stop the superloop; mission tick runs regardless. */
  (void)gps_poll();
  app_mission_tick();
  app_schedule_tick();
  app_camera_verify();
  app_image_tick();
}

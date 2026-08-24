/**
 * @file sdlog.c
 * @brief F6 microSD CSV black-box logger (FatFs).
 *
 * Flight cards: SDHC/SDXC only (block addressing; see sd_spi). Detect pin
 * high = present (roadmap §2). Mounts Cube USERFatFS / USERPath only.
 */

#include "sdlog.h"
#include "error_flags.h"
#include "fatfs.h"
#include "main.h"

#include <stdio.h>
#include <string.h>

static bool sd_is_mounted;
static bool sd_file_open;
static uint8_t sync_counter;

static bool sdlog_fatfs_name_exists(const char *name, void *ctx)
{
  (void)ctx;
  return f_stat(name, NULL) == FR_OK;
}

static void sdlog_fail_soft(void)
{
  if (sd_file_open)
  {
    (void)f_close(&USERFile);
    sd_file_open = false;
  }
  (void)f_mount(NULL, USERPath, 0);
  sd_is_mounted = false;
  error_flags_set_sd_ok(false);
}

bool sdlog_is_ok(void)
{
  return sd_is_mounted;
}

bool sdlog_init(void)
{
  char filename[SDLOG_FLIGHT_NAME_MAX];
  int idx;
  UINT bw;
  size_t hlen;
  const char header[] = "Timestamp_ms,Temp_C,Alt_m\n";

  sd_is_mounted = false;
  sd_file_open = false;
  sync_counter = 0;

  /* F6.2: detect pin HIGH = card present */
  if (HAL_GPIO_ReadPin(microSD_detect_GPIO_Port, microSD_detect_Pin) != GPIO_PIN_SET)
  {
    error_flags_set_sd_ok(false);
    return false;
  }

  if (f_mount(&USERFatFS, USERPath, 1) != FR_OK)
  {
    error_flags_set_sd_ok(false);
    return false;
  }

  idx = sdlog_next_flight_index(sdlog_fatfs_name_exists, NULL);
  if (idx < 0 || !sdlog_format_flight_name((unsigned)idx, filename, sizeof(filename)))
  {
    sdlog_fail_soft();
    return false;
  }

  if (f_open(&USERFile, filename, FA_WRITE | FA_CREATE_NEW) != FR_OK)
  {
    sdlog_fail_soft();
    return false;
  }
  sd_file_open = true;

  hlen = strlen(header);
  if (f_write(&USERFile, header, hlen, &bw) != FR_OK || bw != hlen)
  {
    sdlog_fail_soft();
    return false;
  }

  if (f_sync(&USERFile) != FR_OK)
  {
    sdlog_fail_soft();
    return false;
  }

  sd_is_mounted = true;
  error_flags_set_sd_ok(true);
  return true;
}

bool sdlog_write_sample(uint32_t timestamp_ms, float temp_c, float alt_m)
{
  char buf[64];
  size_t len;
  UINT bw;

  if (!sd_is_mounted)
  {
    return false;
  }

  len = (size_t)snprintf(buf, sizeof(buf),
                         "%lu,%.2f,%.2f\n",
                         (unsigned long)timestamp_ms, temp_c, alt_m);
  if (len == 0U || len >= sizeof(buf))
  {
    return false;
  }

  if (f_write(&USERFile, buf, len, &bw) != FR_OK || bw != len)
  {
    sdlog_fail_soft();
    return false;
  }

  /* F6.3: sync every 10 writes (~50 s at 5 s beacon) */
  if (++sync_counter >= 10)
  {
    if (f_sync(&USERFile) != FR_OK)
    {
      sdlog_fail_soft();
      return false;
    }
    sync_counter = 0;
  }

  return true;
}

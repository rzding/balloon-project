// sdlog.c
#include "sdlog.h"
#include "error_flags.h"
#include "fatfs.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

static FATFS  fs;
static FIL    fil;
static bool   sd_is_mounted = false;
static uint8_t sync_counter = 0;

bool sdlog_is_ok(void) { return sd_is_mounted; }

bool sdlog_init(void) {
    sd_is_mounted = false;
    sync_counter  = 0;

    // F6.2: detect pin HIGH = card present
    if (HAL_GPIO_ReadPin(microSD_detect_GPIO_Port, microSD_detect_Pin) != GPIO_PIN_SET) {
        error_flags_set_sd_ok(false);
        return false;
    }

    if (f_mount(&fs, "", 1) != FR_OK) {
        error_flags_set_sd_ok(false);
        return false;
    }

    // F6.2: find next available FLIGHTxxx.CSV index
    char filename[16];
    int i;
    for (i = 0; i < 1000; i++) {
        snprintf(filename, sizeof(filename), "FLIGHT%03d.CSV", i);
        if (f_stat(filename, NULL) != FR_OK) break;
    }
    if (i == 1000) {
        error_flags_set_sd_ok(false);
        return false;
    }

    if (f_open(&fil, filename, FA_WRITE | FA_CREATE_NEW) != FR_OK) {
        error_flags_set_sd_ok(false);
        return false;
    }

    // Write header only on fresh file
    const char header[] = "Timestamp_ms,Temp_C,Alt_m\n";
    UINT bw;
    size_t hlen = strlen(header);
    if (f_write(&fil, header, hlen, &bw) != FR_OK || bw != hlen) {
        f_close(&fil);
        error_flags_set_sd_ok(false);
        return false;
    }
    f_sync(&fil);

    sd_is_mounted = true;
    error_flags_set_sd_ok(true);
    return true;
}

bool sdlog_write_sample(uint32_t timestamp_ms, float temp_c, float alt_m) {
    // F6.4: fail-soft
    if (!sd_is_mounted) return false;

    char buf[64];
    size_t len = (size_t)snprintf(buf, sizeof(buf),
                                  "%lu,%.2f,%.2f\n",
                                  (unsigned long)timestamp_ms, temp_c, alt_m);

    UINT bw;
    if (f_write(&fil, buf, len, &bw) != FR_OK || bw != len) {
        error_flags_set_sd_ok(false);
        sd_is_mounted = false;  // stop retrying a dead card
        return false;
    }

    // F6.3: sync every 10 writes
    if (++sync_counter >= 10) {
        f_sync(&fil);
        sync_counter = 0;
    }

    return true;
}
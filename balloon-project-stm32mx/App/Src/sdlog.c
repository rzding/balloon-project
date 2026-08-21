#include "sdlog.h"
#include "fatfs.h"
#include "main.h" 
#include <stdio.h>
#include <string.h>

static FATFS fs;
static FIL fil;
static bool sd_is_mounted = false;
static uint8_t sync_counter = 0;

bool sdlog_init(void) {
    // F6.2: Mount policy. Detect pin HIGH = present
    if (HAL_GPIO_ReadPin(microSD_detect_GPIO_Port, microSD_detect_Pin) != GPIO_PIN_SET) {
        return false; 
    }

    if (f_mount(&fs, "", 1) != FR_OK) return false;
    
    // Create or append to FLIGHT.CSV
    if (f_open(&fil, "FLIGHT.CSV", FA_WRITE | FA_OPEN_APPEND) != FR_OK) return false;
    
    char header[] = "Timestamp_ms,Temp_C,Alt_m\n";
    UINT bytesWrote;
    f_write(&fil, header, strlen(header), &bytesWrote);
    f_sync(&fil); 
    
    sd_is_mounted = true;
    return true;
}

bool sdlog_write_sample(uint32_t timestamp, float temp_c, float alt_m) {
    // F6.4: Fail-soft. Do nothing if card isn't mounted.
    if (!sd_is_mounted) return false; 
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%lu,%.2f,%.2f\n", timestamp, temp_c, alt_m);
    
    UINT bytesWrote;
    if (f_write(&fil, buf, strlen(buf), &bytesWrote) != FR_OK) {
        return false;
    }
    
    // F6.3: Force a physical save every 10 loops
    sync_counter++;
    if (sync_counter >= 10) {
        f_sync(&fil);
        sync_counter = 0;
    }
    
    return true;
}
#pragma once
#include <stdbool.h>
#include <stdint.h>

// F6 API
bool sdlog_init(void);
bool sdlog_write_sample(uint32_t timestamp, float temp_c, float alt_m);
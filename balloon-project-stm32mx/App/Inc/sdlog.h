// sdlog.h
#pragma once
#include <stdbool.h>
#include <stdint.h>

bool sdlog_init(void);
bool sdlog_is_ok(void);
bool sdlog_write_sample(uint32_t timestamp_ms, float temp_c, float alt_m);
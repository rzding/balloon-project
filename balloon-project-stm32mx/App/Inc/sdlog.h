/**
 * @file sdlog.h
 * @brief F6 microSD CSV logging API (FatFs append; fail-soft).
 *
 * Flight storage expects SDHC/SDXC industrial microSD (block addressing).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDLOG_FLIGHT_INDEX_MAX 1000u
#define SDLOG_FLIGHT_NAME_MAX  16u

/**
 * @brief Format FLIGHT%03u.CSV for @p index (0..999).
 * @return false if args invalid or @p buflen too small.
 */
static inline bool sdlog_format_flight_name(unsigned index, char *buf, size_t buflen)
{
  int n;

  if (buf == NULL || buflen < 13u || index >= SDLOG_FLIGHT_INDEX_MAX)
  {
    return false;
  }

  n = snprintf(buf, buflen, "FLIGHT%03u.CSV", index);
  return n > 0 && (size_t)n < buflen;
}

/**
 * @brief Next free FLIGHTxxx index using @p exists (true = name already on card).
 * @return 0..999, or -1 if full / bad args.
 */
static inline int sdlog_next_flight_index(bool (*exists)(const char *name, void *ctx),
                                         void *ctx)
{
  char name[SDLOG_FLIGHT_NAME_MAX];
  unsigned i;

  if (exists == NULL)
  {
    return -1;
  }

  for (i = 0; i < SDLOG_FLIGHT_INDEX_MAX; i++)
  {
    if (!sdlog_format_flight_name(i, name, sizeof(name)))
    {
      return -1;
    }
    if (!exists(name, ctx))
    {
      return (int)i;
    }
  }

  return -1;
}

bool sdlog_init(void);
bool sdlog_is_ok(void);
bool sdlog_write_sample(uint32_t timestamp_ms, float temp_c, float alt_m);

#ifdef __cplusplus
}
#endif

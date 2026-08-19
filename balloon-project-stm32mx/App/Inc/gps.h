/**
 * @file gps.h
 * @brief MAX-M10S GPS driver (USART1 @ 9600 8N1).
 *
 * F5.1: RXNE ISR → fixed ring buffer; gps_poll() extracts NMEA lines ending in LF.
 * F5.2: NMEA sentence parser (GGA/RMC).
 * F5.3: gps_has_fix() from status fields.
 * F5.4: Optional UBX baud config (later).
 *
 * Host-testable ring and line helpers have no HAL dependency.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** Ring buffer capacity (power of 2). */
#define GPS_RING_SIZE       256u

/** Max stored NMEA line length (NMEA 0183 max 82 + margin). */
#define GPS_LINE_MAX        128u

/** Ring buffer: single-producer (ISR) / single-consumer (gps_poll). */
typedef struct
{
  volatile uint16_t head;
  volatile uint16_t tail;
  uint8_t data[GPS_RING_SIZE];
} gps_ring_t;

/** Line accumulator state for LF-terminated extraction. */
typedef struct
{
  char buf[GPS_LINE_MAX];
  uint16_t len;
  bool discard;
} gps_line_acc_t;

/**
 * @brief Push one byte into the ring (ISR-safe producer side).
 *
 * On overflow, the oldest byte is dropped (head advances).
 */
static inline void gps_ring_push(gps_ring_t *ring, uint8_t byte)
{
  uint16_t next;

  if (ring == NULL)
  {
    return;
  }

  next = (uint16_t)((ring->head + 1u) & (GPS_RING_SIZE - 1u));
  if (next == ring->tail)
  {
    ring->tail = (uint16_t)((ring->tail + 1u) & (GPS_RING_SIZE - 1u));
  }

  ring->data[ring->head] = byte;
  ring->head = next;
}

/**
 * @brief Pop one byte from the ring (consumer side only).
 * @return false if ring empty; true and *out set on success.
 */
static inline bool gps_ring_pop(gps_ring_t *ring, uint8_t *out)
{
  if (ring == NULL || out == NULL || ring->head == ring->tail)
  {
    return false;
  }

  *out = ring->data[ring->tail];
  ring->tail = (uint16_t)((ring->tail + 1u) & (GPS_RING_SIZE - 1u));
  return true;
}

/**
 * @brief Feed one byte into the line accumulator.
 *
 * On LF, copies the line (without CR/LF) into @p out if not discarded.
 * Oversize lines are discarded until the next LF.
 *
 * @return true when a complete line was extracted into @p out.
 */
static inline bool gps_line_feed(gps_line_acc_t *acc, uint8_t byte, char *out, size_t out_cap)
{
  if (acc == NULL || out == NULL || out_cap == 0u)
  {
    return false;
  }

  if (byte == (uint8_t)'\n')
  {
    bool have_line = false;

    if (!acc->discard && acc->len > 0u)
    {
      size_t copy_len = acc->len;

      if (copy_len >= out_cap)
      {
        copy_len = out_cap - 1u;
      }

      memcpy(out, acc->buf, copy_len);
      out[copy_len] = '\0';

      if (copy_len > 0u && out[copy_len - 1u] == '\r')
      {
        out[copy_len - 1u] = '\0';
      }

      have_line = true;
    }

    acc->len = 0u;
    acc->discard = false;
    return have_line;
  }

  if (acc->discard)
  {
    return false;
  }

  if (acc->len >= (GPS_LINE_MAX - 1u))
  {
    acc->discard = true;
    acc->len = 0u;
    return false;
  }

  acc->buf[acc->len] = (char)byte;
  acc->len++;
  return false;
}

/** NMEA sentence types recognized by the parser (F5.2). */
typedef enum
{
  GPS_NMEA_NONE = 0,
  GPS_NMEA_GGA,
  GPS_NMEA_RMC
} gps_nmea_sentence_type_t;

/** Max comma-separated fields per NMEA line. */
#define GPS_NMEA_MAX_FIELDS  16u

/** Parsed GPS fix fields for LoRa packet v1 / mission use (F5.2). */
typedef struct
{
  int32_t  lat_e7;         /**< Latitude degrees × 1e7 (S/W negative). */
  int32_t  lon_e7;         /**< Longitude degrees × 1e7 (S/W negative). */
  int16_t  alt_m;          /**< GGA altitude, meters. */
  uint8_t  sats;           /**< GGA satellites in use. */
  uint8_t  fix_quality;    /**< GGA fix quality (0 = invalid). */
  bool     rmc_valid;      /**< RMC status field == 'A'. */
  uint32_t time_hhmmss;    /**< UTC time as HHMMSS (no fractional seconds). */
  uint32_t date_ddmmyy;    /**< RMC date DDMMYY; 0 if unknown. */
  bool     lat_lon_valid;  /**< Lat/lon parsed from last GGA or RMC. */
  bool     alt_valid;      /**< Altitude parsed from last GGA. */
} gps_sample_t;

/**
 * @brief Parse two hex digits (0-9, A-F).
 * @return false on invalid digits; true and *out set on success.
 */
static inline bool gps_nmea_hex2(const char *s, uint8_t *out)
{
  uint8_t v;
  uint8_t hi;
  uint8_t lo;

  if (s == NULL || out == NULL || s[0] == '\0' || s[1] == '\0')
  {
    return false;
  }

  hi = (uint8_t)s[0];
  lo = (uint8_t)s[1];

  if (hi >= '0' && hi <= '9')
  {
    v = (uint8_t)(hi - '0');
  }
  else if (hi >= 'A' && hi <= 'F')
  {
    v = (uint8_t)(hi - 'A' + 10u);
  }
  else if (hi >= 'a' && hi <= 'f')
  {
    v = (uint8_t)(hi - 'a' + 10u);
  }
  else
  {
    return false;
  }

  v = (uint8_t)(v << 4);

  if (lo >= '0' && lo <= '9')
  {
    v = (uint8_t)(v + (uint8_t)(lo - '0'));
  }
  else if (lo >= 'A' && lo <= 'F')
  {
    v = (uint8_t)(v + (uint8_t)(lo - 'A' + 10u));
  }
  else if (lo >= 'a' && lo <= 'f')
  {
    v = (uint8_t)(v + (uint8_t)(lo - 'a' + 10u));
  }
  else
  {
    return false;
  }

  *out = v;
  return true;
}

/**
 * @brief Compute NMEA 0183 XOR checksum (bytes after '$' until '*').
 */
static inline uint8_t gps_nmea_checksum_compute(const char *line)
{
  uint8_t cs = 0u;
  const char *p;

  if (line == NULL || line[0] != '$')
  {
    return 0u;
  }

  for (p = line + 1; *p != '\0' && *p != '*'; p++)
  {
    cs ^= (uint8_t)*p;
  }

  return cs;
}

/**
 * @brief Verify NMEA line checksum (*HH).
 */
static inline bool gps_nmea_checksum_ok(const char *line)
{
  const char *star;
  uint8_t expected;
  uint8_t actual;

  if (line == NULL || line[0] != '$')
  {
    return false;
  }

  star = strchr(line, '*');
  if (star == NULL || star[1] == '\0')
  {
    return false;
  }

  if (!gps_nmea_hex2(star + 1, &expected))
  {
    return false;
  }

  actual = gps_nmea_checksum_compute(line);
  return actual == expected;
}

/**
 * @brief Identify GGA/RMC sentence type (talker-agnostic: $xxGGA / $xxRMC).
 */
static inline gps_nmea_sentence_type_t gps_nmea_sentence_type(const char *line)
{
  if (line == NULL || line[0] != '$' || strlen(line) < 7u)
  {
    return GPS_NMEA_NONE;
  }

  if (memcmp(line + 3, "GGA", 3) == 0)
  {
    return GPS_NMEA_GGA;
  }

  if (memcmp(line + 3, "RMC", 3) == 0)
  {
    return GPS_NMEA_RMC;
  }

  return GPS_NMEA_NONE;
}

/**
 * @brief Locate comma-separated field @p index in @p line.
 * @return false if field missing; true and sets @p start and @p len (may be empty).
 */
static inline bool gps_nmea_get_field(const char *line, uint8_t index, const char **start, uint16_t *len)
{
  const char *p;
  const char *field_start;
  uint8_t i;

  if (line == NULL || start == NULL || len == NULL)
  {
    return false;
  }

  p = line;
  field_start = p;
  i = 0u;

  while (true)
  {
    if (*p == ',' || *p == '*' || *p == '\0')
    {
      if (i == index)
      {
        *start = field_start;
        *len = (uint16_t)(p - field_start);
        return true;
      }

      if (*p == '\0' || *p == '*')
      {
        return false;
      }

      i++;
      field_start = p + 1;
    }

    if (*p == '\0')
    {
      return false;
    }

    p++;
  }
}

/**
 * @brief Parse unsigned decimal field (empty => false).
 */
static inline bool gps_nmea_field_to_u32(const char *start, uint16_t len, uint32_t *out)
{
  uint32_t v = 0u;
  uint16_t i;

  if (start == NULL || out == NULL || len == 0u)
  {
    return false;
  }

  for (i = 0u; i < len; i++)
  {
    char c = start[i];

    if (c < '0' || c > '9')
    {
      return false;
    }

    v = (v * 10u) + (uint32_t)(c - '0');
  }

  *out = v;
  return true;
}

/**
 * @brief Parse signed decimal meters from altitude field (integer part only).
 */
static inline bool gps_nmea_field_to_i16_m(const char *start, uint16_t len, int16_t *out)
{
  bool negative = false;
  int32_t v = 0;
  uint16_t i;
  uint16_t begin = 0u;

  if (start == NULL || out == NULL || len == 0u)
  {
    return false;
  }

  if (start[0] == '-')
  {
    negative = true;
    begin = 1u;
    if (len <= 1u)
    {
      return false;
    }
  }

  for (i = begin; i < len; i++)
  {
    char c = start[i];

    if (c == '.')
    {
      break;
    }

    if (c < '0' || c > '9')
    {
      return false;
    }

    v = (v * 10) + (c - '0');
    if (v > 32767)
    {
      return false;
    }
  }

  if (negative)
  {
    v = -v;
  }

  if (v < -32768 || v > 32767)
  {
    return false;
  }

  *out = (int16_t)v;
  return true;
}

/**
 * @brief Convert NMEA ddmm.mmmm (lat) or dddmm.mmmm (lon) to degrees × 1e7.
 *
 * Integer-only; no libm. @p hem must be N/S for latitude, E/W for longitude.
 */
static inline bool gps_nmea_ddmm_to_e7(const char *ddmm, bool is_lat, char hem, int32_t *out)
{
  const char *dot;
  size_t deg_digits;
  size_t ddmm_len;
  int32_t deg = 0;
  int32_t min_whole = 0;
  int32_t min_frac = 0;
  int32_t frac_digits = 0;
  int64_t minutes_times_10000;
  int64_t e7;
  size_t i;

  if (ddmm == NULL || out == NULL || ddmm[0] == '\0')
  {
    return false;
  }

  if (hem != 'N' && hem != 'S' && hem != 'E' && hem != 'W')
  {
    return false;
  }

  ddmm_len = strlen(ddmm);
  dot = strchr(ddmm, '.');
  if (dot == NULL)
  {
    return false;
  }

  deg_digits = is_lat ? 2u : 3u;
  if ((size_t)(dot - ddmm) < deg_digits)
  {
    return false;
  }

  for (i = 0u; i < deg_digits; i++)
  {
    char c = ddmm[i];

    if (c < '0' || c > '9')
    {
      return false;
    }

    deg = (deg * 10) + (c - '0');
  }

  for (i = deg_digits; i < (size_t)(dot - ddmm); i++)
  {
    char c = ddmm[i];

    if (c < '0' || c > '9')
    {
      return false;
    }

    min_whole = (min_whole * 10) + (c - '0');
  }

  for (i = (size_t)(dot - ddmm) + 1u; i < ddmm_len && frac_digits < 4; i++)
  {
    char c = ddmm[i];

    if (c < '0' || c > '9')
    {
      return false;
    }

    min_frac = (min_frac * 10) + (c - '0');
    frac_digits++;
  }

  while (frac_digits < 4)
  {
    min_frac *= 10;
    frac_digits++;
  }

  minutes_times_10000 = ((int64_t)min_whole * 10000LL) + (int64_t)min_frac;
  e7 = ((int64_t)deg * 10000000LL) + ((minutes_times_10000 * 10000000LL) / 600000LL);

  if (hem == 'S' || hem == 'W')
  {
    e7 = -e7;
  }

  if (e7 < (int64_t)INT32_MIN || e7 > (int64_t)INT32_MAX)
  {
    return false;
  }

  *out = (int32_t)e7;
  return true;
}

/**
 * @brief Convert a length-delimited NMEA coordinate field to degrees × 1e7.
 */
static inline bool gps_nmea_field_ddmm_to_e7(const char *start, uint16_t len, bool is_lat, char hem,
                                             int32_t *out)
{
  char buf[16];

  if (start == NULL || out == NULL || len == 0u || len >= sizeof(buf))
  {
    return false;
  }

  memcpy(buf, start, len);
  buf[len] = '\0';
  return gps_nmea_ddmm_to_e7(buf, is_lat, hem, out);
}

/**
 * @brief Parse UTC time field HHMMSS[.sss] into HHMMSS integer.
 */
static inline bool gps_nmea_parse_time_hhmmss(const char *start, uint16_t len, uint32_t *out)
{
  uint32_t v = 0u;
  uint16_t digits = 0u;
  uint16_t i;

  if (start == NULL || out == NULL || len == 0u)
  {
    return false;
  }

  for (i = 0u; i < len; i++)
  {
    char c = start[i];

    if (c == '.')
    {
      break;
    }

    if (c < '0' || c > '9')
    {
      return false;
    }

    v = (v * 10u) + (uint32_t)(c - '0');
    digits++;
  }

  if (digits < 6u)
  {
    return false;
  }

  *out = v;
  return true;
}

/**
 * @brief Parse GGA sentence into @p patch (partial update; does not clear other fields).
 * @return false on wrong type, bad checksum, or parse error; true on success.
 */
static inline bool gps_nmea_parse_gga(const char *line, gps_sample_t *patch)
{
  const char *start;
  uint16_t len;
  uint32_t u32;

  if (line == NULL || patch == NULL)
  {
    return false;
  }

  if (gps_nmea_sentence_type(line) != GPS_NMEA_GGA)
  {
    return false;
  }

  if (!gps_nmea_checksum_ok(line))
  {
    return false;
  }

  if (gps_nmea_get_field(line, 1u, &start, &len) &&
      gps_nmea_parse_time_hhmmss(start, len, &u32))
  {
    patch->time_hhmmss = u32;
  }

  if (gps_nmea_get_field(line, 2u, &start, &len))
  {
    const char *lat_start = start;
    uint16_t lat_len = len;
    const char *hem_start;
    uint16_t hem_len;

    if (gps_nmea_get_field(line, 3u, &hem_start, &hem_len) && hem_len == 1u &&
        gps_nmea_field_ddmm_to_e7(lat_start, lat_len, true, hem_start[0], &patch->lat_e7))
    {
      patch->lat_lon_valid = true;
    }
  }

  if (gps_nmea_get_field(line, 4u, &start, &len))
  {
    const char *lon_start = start;
    uint16_t lon_len = len;
    const char *hem_start;
    uint16_t hem_len;

    if (gps_nmea_get_field(line, 5u, &hem_start, &hem_len) && hem_len == 1u &&
        gps_nmea_field_ddmm_to_e7(lon_start, lon_len, false, hem_start[0], &patch->lon_e7))
    {
      patch->lat_lon_valid = true;
    }
  }

  if (gps_nmea_get_field(line, 6u, &start, &len) &&
      gps_nmea_field_to_u32(start, len, &u32) && u32 <= 255u)
  {
    patch->fix_quality = (uint8_t)u32;
  }

  if (gps_nmea_get_field(line, 7u, &start, &len) &&
      gps_nmea_field_to_u32(start, len, &u32) && u32 <= 255u)
  {
    patch->sats = (uint8_t)u32;
  }

  if (gps_nmea_get_field(line, 9u, &start, &len) &&
      gps_nmea_field_to_i16_m(start, len, &patch->alt_m))
  {
    patch->alt_valid = true;
  }

  return true;
}

/**
 * @brief Parse RMC sentence into @p patch (partial update).
 * @return false on wrong type, bad checksum, or parse error; true on success.
 */
static inline bool gps_nmea_parse_rmc(const char *line, gps_sample_t *patch)
{
  const char *start;
  uint16_t len;
  uint32_t u32;

  if (line == NULL || patch == NULL)
  {
    return false;
  }

  if (gps_nmea_sentence_type(line) != GPS_NMEA_RMC)
  {
    return false;
  }

  if (!gps_nmea_checksum_ok(line))
  {
    return false;
  }

  if (gps_nmea_get_field(line, 1u, &start, &len) &&
      gps_nmea_parse_time_hhmmss(start, len, &u32))
  {
    patch->time_hhmmss = u32;
  }

  if (gps_nmea_get_field(line, 2u, &start, &len) && len == 1u)
  {
    patch->rmc_valid = (start[0] == 'A');
  }

  if (gps_nmea_get_field(line, 3u, &start, &len))
  {
    const char *lat_start = start;
    uint16_t lat_len = len;
    const char *hem_start;
    uint16_t hem_len;

    if (gps_nmea_get_field(line, 4u, &hem_start, &hem_len) && hem_len == 1u &&
        gps_nmea_field_ddmm_to_e7(lat_start, lat_len, true, hem_start[0], &patch->lat_e7))
    {
      patch->lat_lon_valid = true;
    }
  }

  if (gps_nmea_get_field(line, 5u, &start, &len))
  {
    const char *lon_start = start;
    uint16_t lon_len = len;
    const char *hem_start;
    uint16_t hem_len;

    if (gps_nmea_get_field(line, 6u, &hem_start, &hem_len) && hem_len == 1u &&
        gps_nmea_field_ddmm_to_e7(lon_start, lon_len, false, hem_start[0], &patch->lon_e7))
    {
      patch->lat_lon_valid = true;
    }
  }

  if (gps_nmea_get_field(line, 9u, &start, &len) &&
      gps_nmea_field_to_u32(start, len, &u32))
  {
    patch->date_ddmmyy = u32;
  }

  return true;
}

/**
 * @brief Parse GGA or RMC sentence (checksum-validated).
 * @return false if unknown type or bad checksum; true on success.
 */
static inline bool gps_nmea_parse_sentence(const char *line, gps_sample_t *patch)
{
  gps_nmea_sentence_type_t type;

  if (line == NULL || patch == NULL)
  {
    return false;
  }

  type = gps_nmea_sentence_type(line);
  if (type == GPS_NMEA_GGA)
  {
    return gps_nmea_parse_gga(line, patch);
  }

  if (type == GPS_NMEA_RMC)
  {
    return gps_nmea_parse_rmc(line, patch);
  }

  return false;
}

/**
 * @brief Merge parsed patch into accumulated fix state (F5.2).
 *
 * @param is_gga true if patch came from GGA; false for RMC.
 */
static inline void gps_nmea_merge_patch(gps_sample_t *state, const gps_sample_t *patch, bool is_gga)
{
  if (state == NULL || patch == NULL)
  {
    return;
  }

  if (patch->lat_lon_valid)
  {
    state->lat_e7 = patch->lat_e7;
    state->lon_e7 = patch->lon_e7;
    state->lat_lon_valid = true;
  }

  if (is_gga)
  {
    if (patch->alt_valid)
    {
      state->alt_m = patch->alt_m;
      state->alt_valid = true;
    }

    state->sats = patch->sats;
    state->fix_quality = patch->fix_quality;

    if (patch->time_hhmmss != 0u)
    {
      state->time_hhmmss = patch->time_hhmmss;
    }
  }
  else
  {
    state->rmc_valid = patch->rmc_valid;

    if (patch->time_hhmmss != 0u)
    {
      state->time_hhmmss = patch->time_hhmmss;
    }

    if (patch->date_ddmmyy != 0u)
    {
      state->date_ddmmyy = patch->date_ddmmyy;
    }
  }
}

/**
 * @brief Test whether a parsed sample has a valid GPS fix (F5.3).
 *
 * True when lat/lon were parsed and (GGA fix quality >= 1 or RMC status A).
 * Indoor no-lock: quality 0 and RMC V → false. gps_ok / bytes received ≠ fix.
 */
static inline bool gps_sample_has_fix(const gps_sample_t *s)
{
  if (s == NULL || !s->lat_lon_valid)
  {
    return false;
  }

  return (s->fix_quality >= 1u) || s->rmc_valid;
}

/**
 * @brief Arm USART1 RXNE interrupt path and set health flags.
 *
 * gps_ok means RX path armed, not fix acquired (F5.3).
 *
 * @return false on failure to enable RX; true on success.
 */
bool gps_init(void);

/**
 * @brief Drain ring buffer and extract complete LF-terminated lines (non-blocking).
 *
 * @return true if at least one complete line was received this call.
 */
bool gps_poll(void);

/**
 * @brief Last known GPS driver health after gps_init.
 * @return true if RX path is armed; false otherwise.
 */
bool gps_is_ok(void);

/**
 * @brief Copy the most recently completed NMEA line.
 *
 * @param out Destination buffer; must not be NULL.
 * @param cap Capacity of @p out including NUL terminator.
 * @return false on NULL @p out, zero @p cap, or no line received yet; true on success.
 */
bool gps_copy_line(char *out, size_t cap);

/**
 * @brief Copy the merged GPS sample (GGA/RMC fields accumulated in gps_poll).
 *
 * @param out Out sample; must not be NULL.
 * @return false on NULL @p out; true on success.
 */
bool gps_get_sample(gps_sample_t *out);

/**
 * @brief True when merged sample has a valid fix from GGA quality or RMC status.
 *
 * Not the same as gps_is_ok() (RX path armed). Not called from app_run until F8.
 */
bool gps_has_fix(void);

/**
 * @brief USART1 IRQ handler body — push RX bytes into ring; clear overrun.
 *
 * Called from USART1_IRQHandler in stm32f4xx_it.c USER CODE only.
 */
void gps_usart1_irq(void);

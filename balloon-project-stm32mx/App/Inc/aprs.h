/**
 * @file aprs.h
 * @brief DRA818V APRS radio driver (USART2 AT + APRS_PTT + APRS_PD + TIM2 AFSK).
 *
 * F10.1: power idle (PD high / PTT high RX), UART AT config @ 9600 8N1.
 * F10.2: Bell 202 AFSK (TIM2 CH1 PA0) + AX.25 UI / APRS position encode.
 * F10.3: Non-blocking PTT→AFSK→unkey SM + 60 s schedule; APRS_RF_ENABLE gate.
 *
 * Locked init defaults (F10.1 — volume/SQ interim, bench-tunable):
 *   - Handshake AT+DMOCONNECT (retry ≤3)
 *   - Group: 12.5 kHz, TX/RX 144.3900 MHz, CTCSS 0000, SQ=4
 *   - Volume 8 (max MIC drive for AFSK)
 *   - Filters off (pre/de, HP, LP) for clean AFSK — AT+SETFILTER=1,1,1
 *
 * Locked AX.25 / APRS (F10.2 — callsign interim until O6):
 *   - Source APRS_CALLSIGN SSID 11 (balloon); dest APZSSI; path WIDE2-1
 *   - Uncompressed !lat/lonO/A=feet; Bell 202 1200/2200 Hz @ 1200 baud
 *
 * F10.3 sequencing: PTT lead 200 ms → bit play → tail 50 ms → idle.
 * APRS_RF_ENABLE=0 (default): AFSK still plays; PTT stays high (no RF).
 * APRS_RF_ENABLE=1: PTT low during lead/play/tail (licensed RF only).
 *
 * Blocking aprs_afsk_play_bits remains bench/GDB only (always PTT high).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/** DRA818V AT UART baud (datasheet). */
#define APRS_UART_BAUD           9600u

/** Post-PD-wake settle before AT (ms); datasheet 300–500 ms. */
#define APRS_PD_SETTLE_MS        500u

/** Handshake retries before failing init. */
#define APRS_CONNECT_RETRIES     3u

/** Channel space GBW: 0 → 12.5 kHz. */
#define APRS_GBW                 0u

/** Locked APRS US VHF frequency string (MHz, 4 decimal places). */
#define APRS_FREQ_MHZ_STR        "144.3900"

/** CTCSS none. */
#define APRS_CTCSS_NONE          "0000"

/** Squelch 0–8; 4 = datasheet example (interim / bench-tunable). */
#define APRS_SQUELCH             4u

/** Volume 1–8; 8 = max MIC for AFSK (interim / bench-tunable). */
#define APRS_VOLUME              8u

/** AT+SETFILTER args: 1 = off (pre/de, highpass, lowpass). */
#define APRS_FILTER_OFF          1u

/** Max AT command / response line buffer (bytes, incl. NUL). */
#define APRS_AT_BUF_LEN          64u

/* -------------------------------------------------------------------------- */
/* F10.2 — AX.25 / APRS / Bell 202 locked constants                             */
/* -------------------------------------------------------------------------- */

/**
 * Source callsign without SSID (O6 placeholder until licensed callsign).
 * Max 6 characters, AX.25 space-padded.
 */
#define APRS_CALLSIGN            "N0CALL"

/** Balloon APRS SSID (APRS SSID map: -11 airborne). */
#define APRS_SSID                11u

/** Destination TOCALL (experimental APRS software ID). */
#define APRS_DEST_CALL           "APZSSI"

/** Destination SSID (0). */
#define APRS_DEST_SSID           0u

/** Digipeater path callsign. */
#define APRS_DIGI_CALL           "WIDE2"

/** Digipeater SSID (WIDE2-1). */
#define APRS_DIGI_SSID           1u

/** AX.25 UI control field. */
#define APRS_AX25_CTRL_UI        0x03u

/** AX.25 PID: no layer 3. */
#define APRS_AX25_PID_NOL3       0xF0u

/** HDLC flag. */
#define APRS_AX25_FLAG           0x7Eu

/** Leading HDLC flags before frame (TXDELAY-ish). */
#define APRS_AX25_TX_FLAGS       16u

/** Trailing HDLC flags after frame. */
#define APRS_AX25_TAIL_FLAGS     2u

/** Bell 202 mark / space (Hz). */
#define APRS_AFSK_MARK_HZ        1200u
#define APRS_AFSK_SPACE_HZ       2200u

/** AFSK bit rate (baud). */
#define APRS_AFSK_BAUD           1200u

/** F10.3: PTT assert → audio lead-in (ms). */
#define APRS_PTT_LEAD_MS         200u

/** F10.3: audio end → PTT release (ms). */
#define APRS_PTT_TAIL_MS         50u

/** Max bits advanced per aprs_poll (bounds superloop work). */
#define APRS_POLL_BITS_MAX       32u

/** Max APRS info-field length (bytes, excl. NUL). */
#define APRS_INFO_MAX            64u

/**
 * Max raw AX.25 UI frame (addrs + ctrl + pid + info + FCS).
 * 7*3 + 2 + APRS_INFO_MAX + 2.
 */
#define APRS_AX25_FRAME_MAX      (21u + 2u + APRS_INFO_MAX + 2u)

/**
 * Max NRZI bit buffer (one byte per bit for host/play simplicity).
 * Flags + stuffed frame bits with margin.
 */
#define APRS_AX25_BIT_MAX        1600u

/**
 * @brief Format AT+DMOSETGROUP command into @p buf (includes \\r\\n, NUL).
 *
 * @return Bytes written excluding NUL, or -1 on overflow / bad args.
 */
static inline int aprs_format_dmosetgroup(char *buf, size_t cap, uint8_t gbw,
                                          const char *tfv, const char *rfv,
                                          const char *tx_ctcss, uint8_t sq,
                                          const char *rx_ctcss)
{
  int n;

  if (buf == NULL || cap < 2u || tfv == NULL || rfv == NULL ||
      tx_ctcss == NULL || rx_ctcss == NULL || sq > 8u)
  {
    return -1;
  }

  n = snprintf(buf, cap, "AT+DMOSETGROUP=%u,%s,%s,%s,%u,%s\r\n",
               (unsigned)gbw, tfv, rfv, tx_ctcss, (unsigned)sq, rx_ctcss);
  if (n < 0 || (size_t)n >= cap)
  {
    return -1;
  }
  return n;
}

/**
 * @brief Format AT+DMOSETVOLUME command into @p buf (includes \\r\\n, NUL).
 *
 * @return Bytes written excluding NUL, or -1 on overflow / bad volume.
 */
static inline int aprs_format_dmosetvolume(char *buf, size_t cap, uint8_t vol)
{
  int n;

  if (buf == NULL || cap < 2u || vol < 1u || vol > 8u)
  {
    return -1;
  }

  n = snprintf(buf, cap, "AT+DMOSETVOLUME=%u\r\n", (unsigned)vol);
  if (n < 0 || (size_t)n >= cap)
  {
    return -1;
  }
  return n;
}

/**
 * @brief Format AT+SETFILTER command into @p buf (includes \\r\\n, NUL).
 *
 * @return Bytes written excluding NUL, or -1 on overflow.
 */
static inline int aprs_format_setfilter(char *buf, size_t cap, uint8_t pre,
                                        uint8_t hp, uint8_t lp)
{
  int n;

  if (buf == NULL || cap < 2u)
  {
    return -1;
  }

  n = snprintf(buf, cap, "AT+SETFILTER=%u,%u,%u\r\n",
               (unsigned)pre, (unsigned)hp, (unsigned)lp);
  if (n < 0 || (size_t)n >= cap)
  {
    return -1;
  }
  return n;
}

/**
 * @brief True if @p resp contains @p tag followed by ":0" (DRA818V success).
 *
 * Ignores spaces; matches e.g. "+DMOCONNECT:0" or "+DMOCONNECT: 0".
 */
static inline bool aprs_at_ack_ok(const char *resp, const char *tag)
{
  const char *p;
  size_t tag_len;

  if (resp == NULL || tag == NULL)
  {
    return false;
  }

  p = strstr(resp, tag);
  if (p == NULL)
  {
    return false;
  }

  tag_len = strlen(tag);
  p += tag_len;
  if (*p != ':')
  {
    return false;
  }
  p++;
  while (*p == ' ')
  {
    p++;
  }
  return (*p == '0');
}

/* -------------------------------------------------------------------------- */
/* F10.2 — pure AX.25 / APRS encode (host-testable)                             */
/* -------------------------------------------------------------------------- */

/** @brief Absolute value for int32_t (no libc dependency). */
static inline int32_t aprs_i32_abs(int32_t v)
{
  return (v < 0) ? -v : v;
}

/**
 * @brief Convert altitude meters to feet (APRS /A= uses feet MSL).
 * Uses integer approx 3.281 ft/m with rounding toward nearest.
 */
static inline int32_t aprs_alt_m_to_feet(int32_t alt_m)
{
  if (alt_m >= 0)
  {
    return (alt_m * 3281 + 500) / 1000;
  }
  return (alt_m * 3281 - 500) / 1000;
}

/**
 * @brief Format latitude as APRS ddmm.mmN/S (8 chars + NUL).
 *
 * @return Bytes written excluding NUL, or -1 on error.
 */
static inline int aprs_format_lat_aprs(char *out, size_t cap, int32_t lat_e7)
{
  uint32_t abs_e7;
  uint32_t deg;
  uint32_t rem;
  uint32_t min_x100;
  char hemi;
  int n;

  if (out == NULL || cap < 9u)
  {
    return -1;
  }

  hemi = (lat_e7 < 0) ? 'S' : 'N';
  abs_e7 = (uint32_t)aprs_i32_abs(lat_e7);
  deg = abs_e7 / 10000000u;
  rem = abs_e7 % 10000000u;
  min_x100 = (rem * 60u) / 100000u;
  if (deg > 90u)
  {
    return -1;
  }

  n = snprintf(out, cap, "%02u%02u.%02u%c",
               (unsigned)deg, (unsigned)(min_x100 / 100u),
               (unsigned)(min_x100 % 100u), hemi);
  if (n < 0 || (size_t)n >= cap)
  {
    return -1;
  }
  return n;
}

/**
 * @brief Format longitude as APRS dddmm.mmE/W (9 chars + NUL).
 *
 * @return Bytes written excluding NUL, or -1 on error.
 */
static inline int aprs_format_lon_aprs(char *out, size_t cap, int32_t lon_e7)
{
  uint32_t abs_e7;
  uint32_t deg;
  uint32_t rem;
  uint32_t min_x100;
  char hemi;
  int n;

  if (out == NULL || cap < 10u)
  {
    return -1;
  }

  hemi = (lon_e7 < 0) ? 'W' : 'E';
  abs_e7 = (uint32_t)aprs_i32_abs(lon_e7);
  deg = abs_e7 / 10000000u;
  rem = abs_e7 % 10000000u;
  min_x100 = (rem * 60u) / 100000u;
  if (deg > 180u)
  {
    return -1;
  }

  n = snprintf(out, cap, "%03u%02u.%02u%c",
               (unsigned)deg, (unsigned)(min_x100 / 100u),
               (unsigned)(min_x100 % 100u), hemi);
  if (n < 0 || (size_t)n >= cap)
  {
    return -1;
  }
  return n;
}

/**
 * @brief Build uncompressed APRS position info field with balloon symbol /O.
 *
 * Format: !ddmm.mmN/dddmm.mmWO/A=nnnnnn
 *
 * @param alt_m Altitude meters (converted to feet for /A=).
 * @return Bytes written excluding NUL, or -1 on error.
 */
static inline int aprs_format_info_field(char *out, size_t cap, int32_t lat_e7,
                                         int32_t lon_e7, int32_t alt_m)
{
  char lat[9];
  char lon[10];
  int32_t feet;
  int n;

  if (out == NULL || cap < 2u)
  {
    return -1;
  }
  if (aprs_format_lat_aprs(lat, sizeof(lat), lat_e7) < 0)
  {
    return -1;
  }
  if (aprs_format_lon_aprs(lon, sizeof(lon), lon_e7) < 0)
  {
    return -1;
  }

  feet = aprs_alt_m_to_feet(alt_m);
  if (feet < 0)
  {
    feet = 0;
  }
  if (feet > 999999)
  {
    feet = 999999;
  }

  n = snprintf(out, cap, "!%s/%sO/A=%06ld", lat, lon, (long)feet);
  if (n < 0 || (size_t)n >= cap)
  {
    return -1;
  }
  return n;
}

/**
 * @brief Encode one AX.25 address (7 bytes): callsign<<1 + SSID byte.
 *
 * @param last true → set HDLC address-extension end bit (LSB of SSID byte).
 */
static inline bool aprs_ax25_encode_addr(uint8_t out[7], const char *call,
                                         uint8_t ssid, bool last)
{
  size_t i;
  size_t len;

  if (out == NULL || call == NULL || ssid > 15u)
  {
    return false;
  }

  len = strlen(call);
  if (len > 6u)
  {
    return false;
  }

  for (i = 0u; i < 6u; i++)
  {
    char c = (i < len) ? call[i] : ' ';
    if (c >= 'a' && c <= 'z')
    {
      c = (char)(c - 'a' + 'A');
    }
    out[i] = (uint8_t)((uint8_t)c << 1);
  }

  /* C/R=1 (0x60) | SSID<<1 | extension bit */
  out[6] = (uint8_t)(0x60u | ((ssid & 0x0Fu) << 1) | (last ? 0x01u : 0x00u));
  return true;
}

/**
 * @brief AX.25 FCS (CRC-16, poly 0x8408 reflected, init/xor 0xFFFF).
 *
 * Bit order LSB-first per AX.25 / HDLC.
 */
static inline uint16_t aprs_ax25_fcs(const uint8_t *data, size_t len)
{
  uint16_t crc = 0xFFFFu;
  size_t i;
  uint8_t b;

  if (data == NULL && len != 0u)
  {
    return 0u;
  }

  for (i = 0u; i < len; i++)
  {
    uint8_t byte = data[i];
    for (b = 0u; b < 8u; b++)
    {
      uint16_t mix = (uint16_t)((crc ^ (uint16_t)(byte >> b)) & 0x0001u);
      crc = (uint16_t)(crc >> 1);
      if (mix != 0u)
      {
        crc ^= 0x8408u;
      }
    }
  }

  return (uint16_t)(crc ^ 0xFFFFu);
}

/**
 * @brief Build AX.25 UI frame bytes (addresses + UI + PID + info + FCS LE).
 *
 * Uses locked dest/src/digi constants. @p info is raw APRS info (no NUL needed
 * in length @p info_len).
 *
 * @return Frame length in bytes, or 0 on error.
 */
static inline size_t aprs_ax25_build_ui(uint8_t *out, size_t cap,
                                        const char *info, size_t info_len)
{
  size_t n = 0u;
  uint16_t fcs;

  if (out == NULL || info == NULL || info_len == 0u || info_len > APRS_INFO_MAX)
  {
    return 0u;
  }
  if (cap < (21u + 2u + info_len + 2u))
  {
    return 0u;
  }

  if (!aprs_ax25_encode_addr(&out[n], APRS_DEST_CALL, APRS_DEST_SSID, false))
  {
    return 0u;
  }
  n += 7u;
  if (!aprs_ax25_encode_addr(&out[n], APRS_CALLSIGN, APRS_SSID, false))
  {
    return 0u;
  }
  n += 7u;
  if (!aprs_ax25_encode_addr(&out[n], APRS_DIGI_CALL, APRS_DIGI_SSID, true))
  {
    return 0u;
  }
  n += 7u;

  out[n++] = APRS_AX25_CTRL_UI;
  out[n++] = APRS_AX25_PID_NOL3;
  memcpy(&out[n], info, info_len);
  n += info_len;

  fcs = aprs_ax25_fcs(out, n);
  out[n++] = (uint8_t)(fcs & 0xFFu);
  out[n++] = (uint8_t)((fcs >> 8) & 0xFFu);
  return n;
}

/**
 * @brief Append one raw (unstuffed) bit as NRZI into @p bits (0/1 per byte).
 *
 * @param nrzi_level Current NRZI level (0 or 1); updated in place.
 * @return false if @p bit_count would exceed @p bit_cap.
 */
static inline bool aprs_ax25_nrzi_push(uint8_t *bits, size_t bit_cap,
                                       size_t *bit_count, uint8_t *nrzi_level,
                                       uint8_t data_bit)
{
  if (bits == NULL || bit_count == NULL || nrzi_level == NULL)
  {
    return false;
  }
  if (*bit_count >= bit_cap)
  {
    return false;
  }

  /* NRZI: 0 = toggle, 1 = hold */
  if (data_bit == 0u)
  {
    *nrzi_level = (uint8_t)(1u - *nrzi_level);
  }
  bits[(*bit_count)++] = *nrzi_level;
  return true;
}

/**
 * @brief HDLC bit-stuff + NRZI encode of @p frame into one-bit-per-byte buffer.
 *
 * Prepends APRS_AX25_TX_FLAGS flags and appends APRS_AX25_TAIL_FLAGS.
 * Flag bytes are sent without stuffing; frame body is stuffed.
 *
 * @return Number of NRZI bits written, or 0 on error.
 */
static inline size_t aprs_ax25_bitstream(uint8_t *bits, size_t bit_cap,
                                         const uint8_t *frame, size_t frame_len)
{
  size_t bit_count = 0u;
  uint8_t nrzi = 1u; /* idle mark */
  size_t fi;
  uint8_t ones = 0u;

  if (bits == NULL || frame == NULL || frame_len == 0u)
  {
    return 0u;
  }

  /* Leading flags (no stuffing). */
  for (fi = 0u; fi < APRS_AX25_TX_FLAGS; fi++)
  {
    uint8_t b;
    uint8_t flag = APRS_AX25_FLAG;
    ones = 0u;
    for (b = 0u; b < 8u; b++)
    {
      uint8_t bit = (uint8_t)((flag >> b) & 0x01u);
      if (!aprs_ax25_nrzi_push(bits, bit_cap, &bit_count, &nrzi, bit))
      {
        return 0u;
      }
    }
  }

  ones = 0u;
  for (fi = 0u; fi < frame_len; fi++)
  {
    uint8_t byte = frame[fi];
    uint8_t b;
    for (b = 0u; b < 8u; b++)
    {
      uint8_t bit = (uint8_t)((byte >> b) & 0x01u);
      if (!aprs_ax25_nrzi_push(bits, bit_cap, &bit_count, &nrzi, bit))
      {
        return 0u;
      }
      if (bit != 0u)
      {
        ones++;
        if (ones == 5u)
        {
          /* Stuff a 0 after five 1s. */
          if (!aprs_ax25_nrzi_push(bits, bit_cap, &bit_count, &nrzi, 0u))
          {
            return 0u;
          }
          ones = 0u;
        }
      }
      else
      {
        ones = 0u;
      }
    }
  }

  /* Trailing flags. */
  for (fi = 0u; fi < APRS_AX25_TAIL_FLAGS; fi++)
  {
    uint8_t b;
    uint8_t flag = APRS_AX25_FLAG;
    for (b = 0u; b < 8u; b++)
    {
      uint8_t bit = (uint8_t)((flag >> b) & 0x01u);
      if (!aprs_ax25_nrzi_push(bits, bit_cap, &bit_count, &nrzi, bit))
      {
        return 0u;
      }
    }
  }

  return bit_count;
}

/**
 * @brief Build full position UI frame + NRZI bitstream from lat/lon/alt.
 *
 * @param frame_out Optional raw UI frame (may be NULL); @p frame_len_out set.
 * @param bits_out NRZI bit buffer (required).
 * @return Bit count, or 0 on error.
 */
static inline size_t aprs_build_position_frame(uint8_t *frame_out,
                                               size_t frame_cap,
                                               size_t *frame_len_out,
                                               uint8_t *bits_out,
                                               size_t bit_cap, int32_t lat_e7,
                                               int32_t lon_e7, int32_t alt_m)
{
  char info[APRS_INFO_MAX];
  uint8_t frame_local[APRS_AX25_FRAME_MAX];
  uint8_t *frame;
  size_t frame_cap_use;
  int info_len;
  size_t flen;
  size_t blen;

  if (bits_out == NULL || bit_cap == 0u)
  {
    return 0u;
  }

  info_len = aprs_format_info_field(info, sizeof(info), lat_e7, lon_e7, alt_m);
  if (info_len < 0)
  {
    return 0u;
  }

  if (frame_out != NULL && frame_cap > 0u)
  {
    frame = frame_out;
    frame_cap_use = frame_cap;
  }
  else
  {
    frame = frame_local;
    frame_cap_use = sizeof(frame_local);
  }

  flen = aprs_ax25_build_ui(frame, frame_cap_use, info, (size_t)info_len);
  if (flen == 0u)
  {
    return 0u;
  }
  if (frame_len_out != NULL)
  {
    *frame_len_out = flen;
  }

  blen = aprs_ax25_bitstream(bits_out, bit_cap, frame, flen);
  return blen;
}

/**
 * @brief Wake DRA818V, idle PTT RX, configure AT group/volume/filters.
 *
 * Sets error_flags APRS health. Never keys PTT (TX). Fail-soft.
 *
 * @return true on all AT ACKs; false on UART/timeout/ACK failure.
 */
bool aprs_init(void);

/**
 * @brief Last known APRS driver health after aprs_init.
 * @return true if last init succeeded; false otherwise.
 */
bool aprs_is_ok(void);

/**
 * @brief Start continuous Bell 202 tone on TIM2 CH1 (PA0); PTT stays high.
 *
 * @param hz Must be APRS_AFSK_MARK_HZ (1200) or APRS_AFSK_SPACE_HZ (2200).
 * @return false on bad hz or timer failure.
 */
bool aprs_afsk_set_tone(uint16_t hz);

/**
 * @brief Stop TIM2 PWM output (idle low duty / timer stopped).
 */
void aprs_afsk_stop(void);

/**
 * @brief Blocking AFSK playback of NRZI bit buffer (bench/GDB; not app_run).
 *
 * PTT remains high. Each bit is mark (1) or space (0) for 1/1200 s.
 *
 * @return false on NULL/empty or tone programming failure.
 */
bool aprs_afsk_play_bits(const uint8_t *bits, size_t bit_count);

/**
 * @brief Encode position frame and arm non-blocking TX state machine (F10.3).
 *
 * @return false if busy, encode failure, or already transmitting.
 */
bool aprs_tx_start(int32_t lat_e7, int32_t lon_e7, int32_t alt_m);

/**
 * @brief Advance PTT / AFSK state machine; call every app_run (non-blocking).
 */
void aprs_poll(void);

/**
 * @brief True while TX SM is not idle.
 */
bool aprs_tx_busy(void);

/**
 * @file aprs.h
 * @brief DRA818V APRS radio driver (USART2 AT + APRS_PTT + APRS_PD).
 *
 * F10.1: power idle (PD high / PTT high RX), UART AT config @ 9600 8N1.
 * F10.2+: AFSK (TIM2), AX.25, PTT TX sequencing — not in this WP.
 *
 * Locked init defaults (F10.1 — volume/SQ interim, bench-tunable):
 *   - Handshake AT+DMOCONNECT (retry ≤3)
 *   - Group: 12.5 kHz, TX/RX 144.3900 MHz, CTCSS 0000, SQ=4
 *   - Volume 8 (max MIC drive for later AFSK)
 *   - Filters off (pre/de, HP, LP) for clean AFSK — AT+SETFILTER=1,1,1
 *
 * F10.1 never drives PTT low (no RF TX). Params lost on power-off → configure
 * every boot after PD wake settle.
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

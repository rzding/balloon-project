/**
 * @file gps.c
 * @brief MAX-M10S GPS driver — USART1 RX ring, line extract (F5.1), NMEA parse (F5.2), fix validity (F5.3).
 */

#include "gps.h"

#include "error_flags.h"
#include "main.h"

static gps_ring_t s_ring;
static gps_line_acc_t s_line_acc;
static char s_last_line[GPS_LINE_MAX];
static gps_sample_t s_sample;
static bool s_have_line;
static bool s_ok;

/* Bring-up diagnostics (F5 bench).
 *
 * gps_init() cannot detect whether a module is attached -- it only enables the
 * USART1 RX interrupt -- so gps_is_ok() is not evidence of a working GPS.
 * These counters distinguish the failure modes, which need different fixes:
 *
 *   g_gps_rx_bytes == 0            no serial data at all: wiring, power, or
 *                                  TX/RX swapped
 *   g_gps_rx_bytes > 0, lines == 0 bytes arriving but not forming NMEA lines:
 *                                  usually a baud mismatch (garbage bytes)
 *   g_gps_lines > 0, sentences 0   lines received but not GGA/RMC, or failing
 *                                  their checksum
 *   g_gps_sentences > 0, no fix    module is healthy and talking; it simply
 *                                  has no satellite lock yet (needs sky view)
 *
 * External linkage and volatile so they resolve in the debugger from any stop
 * location; nothing in the firmware reads them.
 */
volatile uint32_t g_gps_rx_bytes;
volatile uint32_t g_gps_rx_overruns;
volatile uint32_t g_gps_lines;
volatile uint32_t g_gps_sentences;
char g_gps_last_line[GPS_LINE_MAX];
gps_sample_t g_gps_sample;

static void gps_set_ok(bool ok)
{
  s_ok = ok;
  error_flags_set_gps_ok(ok);
}

static void gps_handle_line(const char *line)
{
  gps_sample_t patch;
  gps_nmea_sentence_type_t type;

  type = gps_nmea_sentence_type(line);
  if (type == GPS_NMEA_NONE)
  {
    return;
  }

  memset(&patch, 0, sizeof(patch));
  if (!gps_nmea_parse_sentence(line, &patch))
  {
    return;
  }

  g_gps_sentences++;
  gps_nmea_merge_patch(&s_sample, &patch, type == GPS_NMEA_GGA);
  g_gps_sample = s_sample;
}

bool gps_init(void)
{
  s_ring.head = 0u;
  s_ring.tail = 0u;
  memset(&s_line_acc, 0, sizeof(s_line_acc));
  memset(s_last_line, 0, sizeof(s_last_line));
  memset(&s_sample, 0, sizeof(s_sample));
  s_have_line = false;

  __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_ERR);

  gps_set_ok(true);
  return true;
}

bool gps_poll(void)
{
  uint8_t byte;
  bool got_line = false;
  char line_buf[GPS_LINE_MAX];

  while (gps_ring_pop(&s_ring, &byte))
  {
    if (gps_line_feed(&s_line_acc, byte, line_buf, sizeof(line_buf)))
    {
      memcpy(s_last_line, line_buf, sizeof(s_last_line));
      memcpy(g_gps_last_line, line_buf, sizeof(g_gps_last_line));
      g_gps_lines++;
      s_have_line = true;
      gps_handle_line(line_buf);
      got_line = true;
    }
  }

  return got_line;
}

bool gps_is_ok(void)
{
  return s_ok;
}

bool gps_copy_line(char *out, size_t cap)
{
  size_t len;

  if (out == NULL || cap == 0u || !s_have_line)
  {
    return false;
  }

  len = strnlen(s_last_line, sizeof(s_last_line));
  if (len >= cap)
  {
    len = cap - 1u;
  }

  memcpy(out, s_last_line, len);
  out[len] = '\0';
  return true;
}

bool gps_get_sample(gps_sample_t *out)
{
  if (out == NULL)
  {
    return false;
  }

  memcpy(out, &s_sample, sizeof(*out));
  return true;
}

bool gps_has_fix(void)
{
  return gps_sample_has_fix(&s_sample);
}

void gps_usart1_irq(void)
{
  uint32_t isr;

  isr = USART1->SR;

  if ((isr & USART_SR_RXNE) != 0u)
  {
    g_gps_rx_bytes++;
    gps_ring_push(&s_ring, (uint8_t)(USART1->DR & 0xFFu));
  }

  if ((isr & USART_SR_ORE) != 0u)
  {
    g_gps_rx_overruns++;
    (void)USART1->DR;
  }
}

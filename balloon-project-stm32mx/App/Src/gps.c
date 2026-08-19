/**
 * @file gps.c
 * @brief MAX-M10S GPS driver — USART1 RX ring and line extraction (F5.1).
 */

#include "gps.h"

#include "error_flags.h"
#include "main.h"

static gps_ring_t s_ring;
static gps_line_acc_t s_line_acc;
static char s_last_line[GPS_LINE_MAX];
static bool s_have_line;
static bool s_ok;

static void gps_set_ok(bool ok)
{
  s_ok = ok;
  error_flags_set_gps_ok(ok);
}

bool gps_init(void)
{
  s_ring.head = 0u;
  s_ring.tail = 0u;
  memset(&s_line_acc, 0, sizeof(s_line_acc));
  memset(s_last_line, 0, sizeof(s_last_line));
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
      s_have_line = true;
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

void gps_usart1_irq(void)
{
  uint32_t isr;

  isr = USART1->SR;

  if ((isr & USART_SR_RXNE) != 0u)
  {
    gps_ring_push(&s_ring, (uint8_t)(USART1->DR & 0xFFu));
  }

  if ((isr & USART_SR_ORE) != 0u)
  {
    (void)USART1->DR;
  }
}

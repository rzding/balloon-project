/**
 * @file aprs.c
 * @brief DRA818V APRS driver — power idle + AT config (F10.1).
 */

#include "aprs.h"

#include <string.h>

#include "error_flags.h"
#include "main.h"

/** Finite HAL UART timeout for one AT exchange (ms). */
#define APRS_UART_TIMEOUT_MS     500u

/** Per-byte RX timeout while assembling a response line (ms). */
#define APRS_UART_BYTE_MS        50u

/** Handshake command (fixed). */
static const char APRS_CMD_CONNECT[] = "AT+DMOCONNECT\r\n";

static bool s_ok;

static void aprs_set_ok(bool ok)
{
  s_ok = ok;
  error_flags_set_aprs_ok(ok);
}

static void aprs_idle_rx(void)
{
  HAL_GPIO_WritePin(APRS_PD_GPIO_Port, APRS_PD_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(APRS_PTT_GPIO_Port, APRS_PTT_Pin, GPIO_PIN_SET);
}

/** Discard any pending RX bytes (short timeout). */
static void aprs_uart_flush_rx(void)
{
  uint8_t b;
  uint32_t guard = 64u;

  while (guard-- > 0u)
  {
    if (HAL_UART_Receive(&huart2, &b, 1u, 2u) != HAL_OK)
    {
      break;
    }
  }
}

/**
 * @brief Read one ASCII line into @p out (strips \\r; stops at \\n or cap-1).
 * @return true if a line ending in \\n was received before overall timeout.
 */
static bool aprs_uart_recv_line(char *out, size_t cap)
{
  size_t n = 0u;
  uint32_t start = HAL_GetTick();

  if (out == NULL || cap < 2u)
  {
    return false;
  }

  out[0] = '\0';

  while ((HAL_GetTick() - start) < APRS_UART_TIMEOUT_MS)
  {
    uint8_t b;
    if (HAL_UART_Receive(&huart2, &b, 1u, APRS_UART_BYTE_MS) != HAL_OK)
    {
      continue;
    }

    if (b == (uint8_t)'\n')
    {
      out[n] = '\0';
      return true;
    }

    if (b == (uint8_t)'\r')
    {
      continue;
    }

    if (n + 1u < cap)
    {
      out[n++] = (char)b;
      out[n] = '\0';
    }
  }

  out[n] = '\0';
  return false;
}

/**
 * @brief Transmit @p cmd and expect an ACK containing @p tag with ":0".
 */
static bool aprs_at_txn(const char *cmd, size_t cmd_len, const char *tag)
{
  char resp[APRS_AT_BUF_LEN];

  if (cmd == NULL || tag == NULL || cmd_len == 0u)
  {
    return false;
  }

  aprs_uart_flush_rx();

  if (HAL_UART_Transmit(&huart2, (uint8_t *)(void *)cmd, (uint16_t)cmd_len,
                        APRS_UART_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }

  if (!aprs_uart_recv_line(resp, sizeof(resp)))
  {
    return false;
  }

  return aprs_at_ack_ok(resp, tag);
}

bool aprs_init(void)
{
  char cmd[APRS_AT_BUF_LEN];
  int n;
  unsigned attempt;

  aprs_set_ok(false);
  aprs_idle_rx();
  HAL_Delay(APRS_PD_SETTLE_MS);

  /* Handshake — datasheet: retry up to 3 times. */
  for (attempt = 0u; attempt < APRS_CONNECT_RETRIES; attempt++)
  {
    if (aprs_at_txn(APRS_CMD_CONNECT, sizeof(APRS_CMD_CONNECT) - 1u,
                    "+DMOCONNECT"))
    {
      break;
    }
  }
  if (attempt >= APRS_CONNECT_RETRIES)
  {
    return false;
  }

  n = aprs_format_dmosetgroup(cmd, sizeof(cmd), APRS_GBW, APRS_FREQ_MHZ_STR,
                              APRS_FREQ_MHZ_STR, APRS_CTCSS_NONE, APRS_SQUELCH,
                              APRS_CTCSS_NONE);
  if (n < 0 || !aprs_at_txn(cmd, (size_t)n, "+DMOSETGROUP"))
  {
    return false;
  }

  n = aprs_format_dmosetvolume(cmd, sizeof(cmd), APRS_VOLUME);
  if (n < 0 || !aprs_at_txn(cmd, (size_t)n, "+DMOSETVOLUME"))
  {
    return false;
  }

  n = aprs_format_setfilter(cmd, sizeof(cmd), APRS_FILTER_OFF, APRS_FILTER_OFF,
                            APRS_FILTER_OFF);
  if (n < 0 || !aprs_at_txn(cmd, (size_t)n, "+DMOSETFILTER"))
  {
    return false;
  }

  /* Remain in RX idle; never key PTT in F10.1. */
  aprs_idle_rx();
  aprs_set_ok(true);
  return true;
}

bool aprs_is_ok(void)
{
  return s_ok;
}

/**
 * @file aprs.c
 * @brief DRA818V APRS driver — AT (F10.1), AFSK (F10.2), PTT SM (F10.3).
 */

#include "aprs.h"

#include <string.h>

#include "error_flags.h"
#include "main.h"

#ifndef APRS_RF_ENABLE
#define APRS_RF_ENABLE 0
#endif

/** Finite HAL UART timeout for one AT exchange (ms). */
#define APRS_UART_TIMEOUT_MS     500u

/** Per-byte RX timeout while assembling a response line (ms). */
#define APRS_UART_BYTE_MS        50u

/** Handshake command (fixed). */
static const char APRS_CMD_CONNECT[] = "AT+DMOCONNECT\r\n";

typedef enum
{
  APRS_SM_IDLE = 0,
  APRS_SM_PTT_LEAD,
  APRS_SM_BIT_PLAY,
  APRS_SM_PTT_TAIL
} aprs_sm_t;

static bool s_ok;
static bool s_pwm_running;

static aprs_sm_t s_sm;
static uint8_t s_bits[APRS_AX25_BIT_MAX];
static size_t s_bit_count;
static size_t s_bit_index;
static uint32_t s_phase_start_ms;
static uint32_t s_bit_deadline_cy;
static uint32_t s_cycles_per_bit;

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

static void aprs_ptt_key_tx(void)
{
#if APRS_RF_ENABLE
  HAL_GPIO_WritePin(APRS_PTT_GPIO_Port, APRS_PTT_Pin, GPIO_PIN_RESET);
#else
  /* Dry-run: keep PTT high (RX); AFSK still driven for scope. */
  HAL_GPIO_WritePin(APRS_PTT_GPIO_Port, APRS_PTT_Pin, GPIO_PIN_SET);
#endif
  HAL_GPIO_WritePin(APRS_PD_GPIO_Port, APRS_PD_Pin, GPIO_PIN_SET);
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

static void aprs_dwt_enable(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0u;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void aprs_delay_cycles(uint32_t cycles)
{
  uint32_t start = DWT->CYCCNT;

  while ((DWT->CYCCNT - start) < cycles)
  {
  }
}

static bool aprs_pwm_apply_hz(uint16_t hz)
{
  uint32_t tim_clk;
  uint32_t arr;
  uint32_t ccr;

  if (hz == 0u)
  {
    return false;
  }

  /* APB1 = HCLK/2 → timer clock x2 → TIM2CLK == SystemCoreClock. */
  tim_clk = SystemCoreClock;
  arr = (tim_clk / (uint32_t)hz);
  if (arr < 2u)
  {
    return false;
  }
  arr -= 1u;
  ccr = (arr + 1u) / 2u;

  __HAL_TIM_DISABLE(&htim2);
  __HAL_TIM_SET_PRESCALER(&htim2, 0u);
  __HAL_TIM_SET_AUTORELOAD(&htim2, arr);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr);
  htim2.Instance->EGR = TIM_EGR_UG;
  __HAL_TIM_ENABLE(&htim2);

  if (!s_pwm_running)
  {
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
    {
      return false;
    }
    s_pwm_running = true;
  }

  return true;
}

static void aprs_sm_abort_to_idle(void)
{
  if (s_pwm_running)
  {
    (void)HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    s_pwm_running = false;
  }
  __HAL_TIM_DISABLE(&htim2);
  aprs_idle_rx();
  s_sm = APRS_SM_IDLE;
  s_bit_count = 0u;
  s_bit_index = 0u;
}

bool aprs_init(void)
{
  char cmd[APRS_AT_BUF_LEN];
  int n;
  unsigned attempt;

  aprs_set_ok(false);
  s_pwm_running = false;
  s_sm = APRS_SM_IDLE;
  s_bit_count = 0u;
  s_bit_index = 0u;
  aprs_idle_rx();
  HAL_Delay(APRS_PD_SETTLE_MS);

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

  aprs_idle_rx();
  aprs_dwt_enable();
  s_cycles_per_bit = SystemCoreClock / APRS_AFSK_BAUD;
  aprs_set_ok(true);
  return true;
}

bool aprs_is_ok(void)
{
  return s_ok;
}

bool aprs_afsk_set_tone(uint16_t hz)
{
  aprs_idle_rx();

  if (hz != APRS_AFSK_MARK_HZ && hz != APRS_AFSK_SPACE_HZ)
  {
    return false;
  }

  return aprs_pwm_apply_hz(hz);
}

void aprs_afsk_stop(void)
{
  if (s_pwm_running)
  {
    (void)HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    s_pwm_running = false;
  }
  __HAL_TIM_DISABLE(&htim2);
  aprs_idle_rx();
}

bool aprs_afsk_play_bits(const uint8_t *bits, size_t bit_count)
{
  size_t i;
  uint32_t cycles_per_bit;

  if (bits == NULL || bit_count == 0u)
  {
    return false;
  }

  /* Bench helper: never key PTT. */
  aprs_idle_rx();
  aprs_dwt_enable();
  cycles_per_bit = SystemCoreClock / APRS_AFSK_BAUD;

  for (i = 0u; i < bit_count; i++)
  {
    uint16_t hz = (bits[i] != 0u) ? APRS_AFSK_MARK_HZ : APRS_AFSK_SPACE_HZ;
    if (!aprs_pwm_apply_hz(hz))
    {
      aprs_afsk_stop();
      return false;
    }
    aprs_delay_cycles(cycles_per_bit);
  }

  aprs_afsk_stop();
  return true;
}

bool aprs_tx_busy(void)
{
  return s_sm != APRS_SM_IDLE;
}

bool aprs_tx_start(int32_t lat_e7, int32_t lon_e7, int32_t alt_m)
{
  size_t n;

  if (s_sm != APRS_SM_IDLE)
  {
    return false;
  }

  n = aprs_build_position_frame(NULL, 0u, NULL, s_bits, sizeof(s_bits), lat_e7,
                                lon_e7, alt_m);
  if (n == 0u)
  {
    return false;
  }

  s_bit_count = n;
  s_bit_index = 0u;
  aprs_dwt_enable();
  s_cycles_per_bit = SystemCoreClock / APRS_AFSK_BAUD;

  aprs_ptt_key_tx();
  s_phase_start_ms = HAL_GetTick();
  s_sm = APRS_SM_PTT_LEAD;
  return true;
}

void aprs_poll(void)
{
  uint32_t advanced;

  switch (s_sm)
  {
    case APRS_SM_IDLE:
      break;

    case APRS_SM_PTT_LEAD:
      if ((HAL_GetTick() - s_phase_start_ms) >= APRS_PTT_LEAD_MS)
      {
        if (s_bit_count == 0u)
        {
          aprs_sm_abort_to_idle();
          break;
        }
        /* Start first bit tone and DWT deadline. */
        {
          uint16_t hz =
              (s_bits[0] != 0u) ? APRS_AFSK_MARK_HZ : APRS_AFSK_SPACE_HZ;
          if (!aprs_pwm_apply_hz(hz))
          {
            aprs_sm_abort_to_idle();
            break;
          }
        }
        s_bit_index = 0u;
        s_bit_deadline_cy = DWT->CYCCNT + s_cycles_per_bit;
        s_sm = APRS_SM_BIT_PLAY;
      }
      break;

    case APRS_SM_BIT_PLAY:
      advanced = 0u;
      while (advanced < APRS_POLL_BITS_MAX)
      {
        int32_t remain = (int32_t)(s_bit_deadline_cy - DWT->CYCCNT);
        if (remain > 0)
        {
          break;
        }

        /* Current bit finished — advance. */
        s_bit_index++;
        if (s_bit_index >= s_bit_count)
        {
          if (s_pwm_running)
          {
            (void)HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
            s_pwm_running = false;
          }
          __HAL_TIM_DISABLE(&htim2);
          s_phase_start_ms = HAL_GetTick();
          s_sm = APRS_SM_PTT_TAIL;
          break;
        }

        {
          uint16_t hz = (s_bits[s_bit_index] != 0u) ? APRS_AFSK_MARK_HZ
                                                    : APRS_AFSK_SPACE_HZ;
          if (!aprs_pwm_apply_hz(hz))
          {
            aprs_sm_abort_to_idle();
            break;
          }
        }
        s_bit_deadline_cy += s_cycles_per_bit;
        advanced++;
      }
      break;

    case APRS_SM_PTT_TAIL:
      if ((HAL_GetTick() - s_phase_start_ms) >= APRS_PTT_TAIL_MS)
      {
        aprs_idle_rx();
        s_sm = APRS_SM_IDLE;
        s_bit_count = 0u;
        s_bit_index = 0u;
      }
      break;

    default:
      aprs_sm_abort_to_idle();
      break;
  }
}

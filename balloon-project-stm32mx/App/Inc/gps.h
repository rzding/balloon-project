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
 * @brief USART1 IRQ handler body — push RX bytes into ring; clear overrun.
 *
 * Called from USART1_IRQHandler in stm32f4xx_it.c USER CODE only.
 */
void gps_usart1_irq(void);

/**
 * @file telem_img.h
 * @brief Image downlink packets (types 0x10 header, 0x11 chunk) over LoRa.
 *
 * A JPEG is captured to RAM on the flight computer, then streamed as one header
 * packet followed by N data chunks. The ground station reassembles by
 * (img_id, chunk_idx) and writes the JPEG once every chunk has arrived.
 *
 * LoRa is a slow image channel (SF8 ~ a few kbit/s); a 160x120 JPEG of a few kB
 * takes ~10-20 s. This is a bench / novelty downlink — SD is the real image path.
 *
 * Header (type 0x10, 16 bytes; CRC over 0..13):
 *   [0] 0x10  [1] img_id  [2..5] total_len  [6..7] total_chunks
 *   [8..9] width  [10..11] height  [12..13] chunk_data_size  [14..15] CRC
 *
 * Chunk (type 0x11, 5 + data + 2 bytes; CRC over 0 .. 4+data_len):
 *   [0] 0x11  [1] img_id  [2..3] chunk_idx  [4] data_len
 *   [5 ..]   data_len JPEG bytes    [..] CRC
 *
 * Header-only, reuses packet.h wire + CRC helpers, no HAL.
 */

#pragma once

#include <stdint.h>

#include "packet.h"

#define TELEM_IMG_HDR_TYPE     0x10u
#define TELEM_IMG_CHUNK_TYPE   0x11u

#define TELEM_IMG_HDR_LEN      16u
#define TELEM_IMG_HDR_CRC_LEN  14u

/** JPEG data bytes per chunk (chunk wire len = 5 + this + 2 <= LoRa 255). */
#define TELEM_IMG_CHUNK_DATA   192u
#define TELEM_IMG_CHUNK_OVH    7u      /* type+id+idx(2)+len + CRC(2) */
#define TELEM_IMG_CHUNK_MAX    (TELEM_IMG_CHUNK_DATA + TELEM_IMG_CHUNK_OVH)

/** @brief Pack the image header packet (16 bytes). */
static inline void telem_img_pack_header(uint8_t out[TELEM_IMG_HDR_LEN], uint8_t img_id,
                                         uint32_t total_len, uint16_t total_chunks,
                                         uint16_t width, uint16_t height, uint16_t chunk_data)
{
  uint16_t crc;

  if (out == NULL)
  {
    return;
  }

  out[0] = TELEM_IMG_HDR_TYPE;
  out[1] = img_id;
  packet_be_u32_write(&out[2], total_len);
  packet_be_u16_write(&out[6], total_chunks);
  packet_be_u16_write(&out[8], width);
  packet_be_u16_write(&out[10], height);
  packet_be_u16_write(&out[12], chunk_data);

  crc = packet_crc16(out, TELEM_IMG_HDR_CRC_LEN);
  packet_be_u16_write(&out[14], crc);
}

/**
 * @brief Pack one image data chunk.
 * @return Wire length (5 + data_len + 2), or 0 on bad args.
 */
static inline uint8_t telem_img_pack_chunk(uint8_t *out, uint8_t img_id, uint16_t chunk_idx,
                                           const uint8_t *data, uint8_t data_len)
{
  uint16_t crc;
  uint8_t i;
  uint8_t crc_at;

  if (out == NULL || data == NULL || data_len == 0u || data_len > TELEM_IMG_CHUNK_DATA)
  {
    return 0u;
  }

  out[0] = TELEM_IMG_CHUNK_TYPE;
  out[1] = img_id;
  packet_be_u16_write(&out[2], chunk_idx);
  out[4] = data_len;
  for (i = 0u; i < data_len; i++)
  {
    out[5u + i] = data[i];
  }

  crc_at = (uint8_t)(5u + data_len);
  crc = packet_crc16(out, crc_at);
  packet_be_u16_write(&out[crc_at], crc);

  return (uint8_t)(crc_at + 2u);
}

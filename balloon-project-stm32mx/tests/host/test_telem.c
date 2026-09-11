/*
 * test_telem — IMU (0x02) and image (0x10/0x11) packet pack + CRC round-trip.
 *
 * Verifies the flight-side telem_imu.h / telem_img.h wire format is
 * self-consistent and that the trailing CRC-16/CCITT-FALSE (the same check the
 * ground station applies) validates. Guards offsets, lengths, and the chunk
 * length rules. Pure logic, no HAL.
 *
 * Build+run:  cd balloon-project-stm32mx/tests/host && make test_telem && ./test_telem
 */
#include <stdint.h>
#include <stdio.h>

#include "telem_img.h"
#include "telem_imu.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL: %s (line %d)\n", #c, __LINE__); fails++; } } while (0)

/* Mirrors the ground .ino crc_ok(): CRC over all but the last two bytes. */
static int crc_tail_ok(const uint8_t *b, int n)
{
  uint16_t stored = (uint16_t)(((uint16_t)b[n - 2] << 8) | b[n - 1]);
  return stored == packet_crc16(b, (uint16_t)(n - 2));
}

int main(void)
{
  imu_sample_t s = {2048, -100, 50, 1, -2, 3};
  uint8_t iw[TELEM_IMU_LEN];
  uint8_t hd[TELEM_IMG_HDR_LEN];
  uint8_t data[TELEM_IMG_CHUNK_DATA];
  uint8_t ck[TELEM_IMG_CHUNK_MAX];
  uint8_t plen;
  unsigned i;

  /* IMU packet */
  telem_imu_pack(iw, 4u, 0xBEEFu, 0x11223344u, &s);
  CHECK(iw[0] == TELEM_IMU_TYPE);
  CHECK(crc_tail_ok(iw, TELEM_IMU_LEN));
  CHECK(iw[1] == 4u);
  CHECK(packet_be_u16_read(&iw[2]) == 0xBEEFu);
  CHECK(packet_be_u32_read(&iw[4]) == 0x11223344u);
  CHECK(packet_be_i16_read(&iw[8]) == 2048);
  CHECK(packet_be_i16_read(&iw[10]) == -100);
  CHECK(packet_be_i16_read(&iw[12]) == 50);
  CHECK(packet_be_i16_read(&iw[18]) == 3);

  /* image header */
  telem_img_pack_header(hd, 7u, 5000u, 27u, 160u, 120u, TELEM_IMG_CHUNK_DATA);
  CHECK(hd[0] == TELEM_IMG_HDR_TYPE);
  CHECK(crc_tail_ok(hd, TELEM_IMG_HDR_LEN));
  CHECK(hd[1] == 7u);
  CHECK(packet_be_u32_read(&hd[2]) == 5000u);
  CHECK(packet_be_u16_read(&hd[6]) == 27u);
  CHECK(packet_be_u16_read(&hd[8]) == 160u);
  CHECK(packet_be_u16_read(&hd[10]) == 120u);
  CHECK(packet_be_u16_read(&hd[12]) == TELEM_IMG_CHUNK_DATA);

  /* full-size chunk */
  for (i = 0u; i < TELEM_IMG_CHUNK_DATA; i++) { data[i] = (uint8_t)i; }
  plen = telem_img_pack_chunk(ck, 7u, 300u, data, (uint8_t)TELEM_IMG_CHUNK_DATA);
  CHECK(plen == (uint8_t)(TELEM_IMG_CHUNK_DATA + 7u));
  CHECK(ck[0] == TELEM_IMG_CHUNK_TYPE);
  CHECK(crc_tail_ok(ck, plen));
  CHECK(packet_be_u16_read(&ck[2]) == 300u);
  CHECK(ck[4] == (uint8_t)TELEM_IMG_CHUNK_DATA);
  CHECK(ck[5] == 0u && ck[5u + 191u] == 191u);

  /* short chunk */
  plen = telem_img_pack_chunk(ck, 7u, 26u, data, 8u);
  CHECK(plen == 15u);
  CHECK(crc_tail_ok(ck, plen));
  CHECK(ck[4] == 8u);

  /* invalid: oversize and zero-length rejected */
  CHECK(telem_img_pack_chunk(ck, 7u, 0u, data, 255u) == 0u);
  CHECK(telem_img_pack_chunk(ck, 7u, 0u, data, 0u) == 0u);

  printf(fails ? "test_telem: FAIL (%d)\n" : "test_telem: PASS\n", fails);
  return fails ? 1 : 0;
}

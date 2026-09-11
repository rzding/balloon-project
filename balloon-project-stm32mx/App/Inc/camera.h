/**
 * @file camera.h
 * @brief ArduCAM Mini 2MP (ArduChip + OV2640) driver — F9.1 bus bring-up (SPI1 + I2C1 + Cam_CS).
 *
 * F9.1a (this revision) is probe-only: known-answer tests on both camera buses,
 * no sensor configuration and no capture.
 *   - SPI1 + Cam_CS: ArduChip TEST1 scratch register write/readback (0x55, then
 *     0xAA), then the ArduChip REV register (recorded, not gated).
 *   - I2C1: OV2640 SCCB address ACK, sensor-bank select, PIDH/PIDL and
 *     MIDH/MIDL identity (0x26 0x41|0x42, 0x7F 0xA2).
 * F9.1b: sensor init tables (JPEG, resolution). F9.2: FIFO capture → SD.
 * F9.3: mission hook (app_camera_on_due).
 *
 * Board wiring (schematic + production netlist, CAM_CONN J9):
 *   1 CS  2 MOSI  3 MISO  4 SCK  5 GND  6 +5V  7 SDA  8 SCL
 *   Cam_CS = PA4 with a 10k pull-up to +5V (R27). SCL/SDA = PB6/PB7 with 4.7k
 *   pull-ups to +3V3 (R28/R29) — the MCU adds no pull-ups. Module runs from the
 *   5 V rail; the datasheet rates all I/O 3.3 V / 5 V tolerant.
 *
 * ArduChip SPI: mode 0 (bus default), <= 8 MHz. Address byte bit 7 = 1 for
 * write, 0 for read — the inverse of the ICM convention in spi_bus_read_reg8 /
 * spi_bus_write_reg8, so this driver frames its own transfers (like lora.c).
 *
 * OV2640 SCCB: 7-bit address 0x30. Register 0xFF selects the bank (0x01 =
 * sensor bank, where PID/MID live; 0x00 = DSP bank). Reads are
 * write-register, STOP, then a one-byte read — no repeated start — matching
 * the ArduCAM reference library.
 *
 * Shared-bus caveat: some ArduCAM Mini revisions do not release MISO while CS
 * is high. If plugging the camera in makes the IMU/baro/temp/LoRa identity
 * checks fail, that is the first thing to suspect (see roadmap §18).
 *
 * Bench/GDB: every stage mirrors into volatile g_cam_* globals in camera.c;
 * the first wrong value names the failing rung (ladder in camera.c header).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** OV2640 SCCB 7-bit address 0x30 in HAL 8-bit form (0x60 write / 0x61 read). */
#define CAMERA_OV2640_I2C_ADDR      0x60u

/** OV2640 identity (sensor bank): PIDH 0x0A, PIDL 0x0B, MIDH 0x1C, MIDL 0x1D. */
#define CAMERA_OV2640_PIDH_EXPECT   0x26u
#define CAMERA_OV2640_PIDL_EXPECT_A 0x41u   /* OV2640 */
#define CAMERA_OV2640_PIDL_EXPECT_B 0x42u   /* later OV2640 silicon */
#define CAMERA_OV2640_MIDH_EXPECT   0x7Fu   /* OmniVision manufacturer ID 0x7FA2 */
#define CAMERA_OV2640_MIDL_EXPECT   0xA2u

/**
 * I2C1 bring-up clock (Hz). CubeMX configures 400 kHz; camera_init re-inits
 * I2C1 to this conservative rate (same policy as the SPI bring-up prescaler).
 * Raise only once the sensor is proven at speed. I2C1 has no other slave.
 */
#define CAMERA_I2C_CLOCK_HZ         100000u

/**
 * Write 0x80 then 0x00 to ArduChip register 0x07 before probing — the vendor
 * example sequence for Plus-revision ArduChips. Earlier revisions do not
 * implement 0x07, where the writes are no-ops. Costs 200 ms at boot.
 */
#define CAMERA_CPLD_RESET_ON_INIT   1

/** Probe stages; the g_cam_*_fail_stage globals hold the first failure. */
typedef enum
{
  CAMERA_STAGE_PASS = 0,        /* probe completed, every known answer matched */
  CAMERA_STAGE_SPI_TEST_A,      /* TEST1 write/readback 0x55 */
  CAMERA_STAGE_SPI_TEST_B,      /* TEST1 write/readback 0xAA */
  CAMERA_STAGE_SPI_REV,         /* REV register read: SPI transfer error */
  CAMERA_STAGE_I2C_CLOCK,       /* HAL_I2C_Init at CAMERA_I2C_CLOCK_HZ failed */
  CAMERA_STAGE_I2C_ACK,         /* OV2640 did not ACK address 0x30 */
  CAMERA_STAGE_I2C_BANK,        /* bank-select write failed */
  CAMERA_STAGE_I2C_PID,         /* PIDH/PIDL read failed or mismatched */
  CAMERA_STAGE_I2C_MID,         /* MIDH/MIDL read failed or mismatched */
  CAMERA_STAGE_NOT_RUN          /* camera_init has not executed yet */
} camera_stage_t;

/** @brief Pure check: do PIDH/PIDL identify an OV2640? */
static inline bool camera_ov2640_pid_ok(uint8_t pidh, uint8_t pidl)
{
  return (pidh == CAMERA_OV2640_PIDH_EXPECT) &&
         ((pidl == CAMERA_OV2640_PIDL_EXPECT_A) ||
          (pidl == CAMERA_OV2640_PIDL_EXPECT_B));
}

/** @brief Pure check: MIDH/MIDL equal the OmniVision manufacturer ID 0x7FA2. */
static inline bool camera_ov2640_mid_ok(uint8_t midh, uint8_t midl)
{
  return (midh == CAMERA_OV2640_MIDH_EXPECT) && (midl == CAMERA_OV2640_MIDL_EXPECT);
}

/**
 * @brief Probe both camera buses (F9.1a).
 *
 * Runs the SPI probe and the I2C probe independently so one boot diagnoses
 * both buses. Fail-soft: finite HAL timeouts everywhere, no retry loops;
 * worst case is under 1.5 s if every HAL call times out, typical about
 * 0.2 s (dominated by the CPLD reset settle time). Sets error_flags cam_ok = (spi ok && i2c ok).
 *
 * @return true only if the ArduChip and the OV2640 both answered correctly.
 */
bool camera_init(void);

/** @brief Last camera_init verdict (both buses). */
bool camera_is_ok(void);

/** @brief ArduChip reachable: TEST1 write/readback passed with both patterns. */
bool camera_spi_ok(void);

/** @brief OV2640 identified over I2C (ACK + PID + MID). */
bool camera_i2c_ok(void);

/** @brief ArduChip REV register (0x40) from the last probe; 0 if never read. */
uint8_t camera_get_arduchip_rev(void);

/** @brief (PIDH << 8) | PIDL from the last probe; 0 if never read. */
uint16_t camera_get_sensor_pid(void);

/** @brief First failing SPI stage, CAMERA_STAGE_PASS, or CAMERA_STAGE_NOT_RUN. */
camera_stage_t camera_get_spi_fail_stage(void);

/** @brief First failing I2C stage, CAMERA_STAGE_PASS, or CAMERA_STAGE_NOT_RUN. */
camera_stage_t camera_get_i2c_fail_stage(void);

/* ---- F9.1b / F9.2: JPEG configuration and ArduChip FIFO capture ---------- */

/** JPEG capture resolution. */
typedef enum
{
  CAMERA_RES_160x120 = 0,   /* small, ~a few kB — practical over LoRa */
  CAMERA_RES_320x240 = 1
} camera_res_t;

/** Pixel dimensions for a resolution (for the image header packet). */
static inline uint16_t camera_res_width(camera_res_t r)
{
  return (r == CAMERA_RES_320x240) ? 320u : 160u;
}
static inline uint16_t camera_res_height(camera_res_t r)
{
  return (r == CAMERA_RES_320x240) ? 240u : 120u;
}

/**
 * @brief Configure the OV2640 for JPEG output at @p res (F9.1b).
 *
 * Requires a prior camera_init with I2C ok. Soft-resets the sensor and writes
 * the ArduCAM OV2640 JPEG init + format + resolution tables over SCCB (~250
 * register writes, ~0.5 s including AE/AWB settle). Idempotent; safe to call
 * once before the first capture.
 *
 * @return false if I2C is not up or any SCCB write fails.
 */
bool camera_configure_jpeg(camera_res_t res);

/** @brief True once camera_configure_jpeg has succeeded. */
bool camera_jpeg_ready(void);

/** @brief Resolution set by the last successful camera_configure_jpeg. */
camera_res_t camera_get_res(void);

/**
 * @brief Capture one JPEG frame into @p buf.
 *
 * Clears the ArduChip FIFO, triggers a capture, waits for CAP_DONE (finite
 * timeout), reads the FIFO length, and burst-reads min(length, buf_max) bytes.
 * Blocking; holds the SPI bus for the burst, so no other slave runs meanwhile.
 * The image is truncated (still partially decodable) if it exceeds @p buf_max.
 *
 * @param buf Destination buffer.
 * @param buf_max Buffer capacity in bytes.
 * @param out_len Out: bytes actually read.
 * @return false on bad args, capture timeout, empty FIFO, or SPI failure.
 */
bool camera_capture(uint8_t *buf, uint32_t buf_max, uint32_t *out_len);

/** @brief FIFO length reported by the last camera_capture (bench/GDB). */
uint32_t camera_get_last_fifo_len(void);

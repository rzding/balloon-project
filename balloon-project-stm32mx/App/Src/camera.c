/**
 * @file camera.c
 * @brief ArduCAM Mini 2MP driver — F9.1a SPI (ArduChip) and I2C (OV2640) bus probes.
 *
 * Bench diagnostic ladder — read the g_cam_* globals in GDB once app_init has
 * returned (Suspend any time after boot). The first wrong value names the rung.
 *
 *   SPI (ArduChip behind Cam_CS):
 *     g_cam_spi_xfer_fail    spi_bus_transfer failures (HAL error/timeout) → want 0
 *     g_cam_spi_rd_a         readback after writing 0x55 to TEST1        → want 0x55
 *     g_cam_spi_rd_b         readback after writing 0xAA to TEST1        → want 0xAA
 *     g_cam_arduchip_rev     REV register 0x40 (informational; record it)
 *     g_cam_spi_fail_stage   0 = pass, else the first failing camera_stage_t
 *       both readbacks 0x00 or 0xFF → no slave answering: CS to J9-1, +5V on
 *                                     J9-6, MISO; the read sends 0x00 on MOSI,
 *                                     so an echo or a floating line cannot pass
 *       one pattern right, one wrong → marginal clocking: SCK/MOSI, SPI mode 0
 *
 *   I2C (OV2640 SCCB at 0x30 on PB6 = SCL, PB7 = SDA):
 *     g_cam_i2c_lines_idle   bit1 SCL, bit0 SDA sampled at entry            → want 3
 *     g_cam_i2c_line_test    GPIO drive test (ST errata recovery sequence)  → want 0x1F
 *                              0x01 SDA high when released
 *                              0x02 SCL high when released
 *                              0x04 SDA reads low while the MCU drives it low
 *                              0x08 SCL reads low while the MCU drives it low
 *                              0x10 both high again after the STOP
 *                            missing 0x04 or 0x08 → that line cannot be pulled
 *                            low: short to 3V3/5V, or a push-pull driver on it
 *     g_cam_i2c_line_test2   the same test run again before the filter-off retry
 *     g_cam_i2c_drive_dbg    snapshot while both lines are driven low:
 *                              bits[1:0] IDR (bit1 SDA, bit0 SCL)  → want 0 if the pad wins
 *                              bits[5:4] ODR                        → want 0 (driver on)
 *                              bits[11:8] MODER[7:6]                → want 0x5 (both output)
 *                              bits[13:12] OTYPER[7:6]              → want 0x3 (open-drain)
 *                            0x3503 = driver on, pads still high: wire held high externally
 *                            0x3500 = driver on and the pads went low: pass
 *     g_cam_i2c_recoveries   times SDA was found held low and clocked free
 *     g_cam_i2c_pin_cfg      PB6/PB7 config after re-init                  → want 0x443A
 *                            (AF7 = 4, AF6 = 4, both open-drain, both AF mode)
 *     g_cam_i2c_attempt      1 = HAL probe passed with the analog filter on
 *                            2 = passed only with the filter off (F4 errata)
 *                            0 = neither
 *     g_cam_i2c_ack          1 if the sensor ACKed its address on the HAL path
 *     g_cam_i2c_hal_status   HAL status of the last failing call
 *     g_cam_i2c_hal_err      hi2c1.ErrorCode at that failure
 *       status 1, err 0      → START went out, nobody ACKed 0x30 (wiring / module)
 *       status 3, err 0x200  → START never generated (HAL_I2C_WRONG_START):
 *                              MCU could not pull SDA low, or peripheral locked
 *       status 2             → BUSY flag stuck
 *       err 0x04 (AF) later  → the sensor stopped ACKing mid-probe
 *     g_cam_i2c_cr1 / g_cam_i2c_sr1 / g_cam_i2c_sr2   I2C1 registers at first failure
 *     g_cam_bb_ack           bit-banged SCCB fallback (GPIO only, no I2C peripheral;
 *                            runs only when the HAL path fails): sensor ACKed 0x30
 *     g_cam_bb_pidh / g_cam_bb_pidl   bit-banged PID                  → 0x26 / 0x41|0x42
 *       bb_ack = 1, HAL fails         → wiring and sensor fine; STM32 I2C1 path at fault
 *       bb_ack = 0, line_test = 0x1F  → lines fine, sensor silent: SDA/SCL swapped,
 *                                       or the module's sensor is unpowered
 *     g_cam_pidh / g_cam_pidl   → want 0x26 / 0x41 or 0x42
 *     g_cam_midh / g_cam_midl   → want 0x7F / 0xA2
 *     g_cam_mid_ok            1 = MID matched; 0 = didn't (informational — PID gates OK)
 *     g_cam_i2c_fail_stage   0 = pass, else the first failing camera_stage_t
 */

#include "camera.h"

#include "error_flags.h"
#include "main.h"
#include "spi_bus.h"

#include "camera_ov2640_regs.h"

/* CubeMX-owned I2C1 handle (Core/Src/main.c). */
extern I2C_HandleTypeDef hi2c1;

/* I2C1 pins per the .ioc (PB6 = I2C1_SCL, PB7 = I2C1_SDA); no Cube label exists. */
#define CAM_I2C_GPIO_PORT           GPIOB
#define CAM_I2C_SCL_PIN             GPIO_PIN_6
#define CAM_I2C_SDA_PIN             GPIO_PIN_7
#define CAM_I2C_SCL_BIT             6u
#define CAM_I2C_SDA_BIT             7u

/** ArduChip register map (ArduCAM reference library, ArduCAM.h). */
#define CAM_REG_TEST1               0x00u   /* scratch register — bus test */
#define CAM_REG_CAPTURE_CTRL        0x01u   /* bits[2:0]: frames per capture */
#define CAM_REG_MODE                0x02u
#define CAM_REG_TIM                 0x03u
#define CAM_REG_FIFO                0x04u   /* CLEAR 0x01, START 0x02, RDPTR_RST 0x10, WRPTR_RST 0x20 */
#define CAM_REG_GPIO                0x06u   /* sensor reset/pwdn/pwren (Plus revisions) */
#define CAM_REG_CPLD                0x07u   /* bit 7: ArduChip reset (Plus revisions) */
#define CAM_REG_BURST_FIFO_READ     0x3Cu
#define CAM_REG_SINGLE_FIFO_READ    0x3Du
#define CAM_REG_REV                 0x40u   /* ArduChip revision */
#define CAM_REG_TRIG                0x41u   /* VSYNC 0x01, SHUTTER 0x02, CAP_DONE 0x08 */
#define CAM_REG_FIFO_SIZE1          0x42u   /* FIFO length [7:0] */
#define CAM_REG_FIFO_SIZE2          0x43u   /* [15:8] */
#define CAM_REG_FIFO_SIZE3          0x44u   /* [18:16] */

/** ArduChip FIFO control bits (CAM_REG_FIFO). */
#define CAM_FIFO_CLEAR              0x01u
#define CAM_FIFO_START             0x02u
/** ArduChip TRIG bits (CAM_REG_TRIG). */
#define CAM_TRIG_CAP_DONE          0x08u
/** FIFO length is 18-bit. */
#define CAM_FIFO_LEN_MASK          0x07FFFFu
/** Capture wait ceiling (ms). */
#define CAM_CAPTURE_TIMEOUT_MS     3000u
/** Burst-read block (bytes per HAL receive; CS held across all blocks). */
#define CAM_FIFO_BLOCK             1024u
/** OV2640 AE/AWB settle after JPEG config (ms). */
#define CAM_JPEG_SETTLE_MS         300u
/** OV2640 COM7 register and soft-reset bit. */
#define CAM_OV2640_REG_COM7        0x12u
#define CAM_OV2640_COM7_SRST       0x80u

/** ArduChip address byte: bit 7 = 1 write, 0 read. */
#define CAM_SPI_WRITE_BIT           0x80u

/** Bytes per single register access (address + data). */
#define CAM_REG_TRANSFER_BYTES      2u

/** Finite HAL SPI timeout for register access. */
#define CAM_SPI_TIMEOUT_MS          100u

#define CAM_TEST_PATTERN_A          0x55u
#define CAM_TEST_PATTERN_B          0xAAu

#define CAM_CPLD_RESET_BIT          0x80u
#define CAM_CPLD_RESET_SETTLE_MS    100u    /* vendor examples: 100 ms after each edge */

/** OV2640 SCCB registers. */
#define CAM_OV2640_REG_BANK_SEL     0xFFu
#define CAM_OV2640_BANK_SENSOR      0x01u
#define CAM_OV2640_REG_PIDH         0x0Au
#define CAM_OV2640_REG_PIDL         0x0Bu
#define CAM_OV2640_REG_MIDH         0x1Cu
#define CAM_OV2640_REG_MIDL         0x1Du

/** OV2640 7-bit address, and the R/W bit, for the bit-banged path. */
#define CAM_OV2640_ADDR_W           CAMERA_OV2640_I2C_ADDR
#define CAM_OV2640_ADDR_R           (CAMERA_OV2640_I2C_ADDR | 0x01u)

/** Finite HAL I2C timeouts. */
#define CAM_I2C_TIMEOUT_MS          50u
#define CAM_SCCB_READ_SETTLE_MS      1u    /* OV2640 SCCB likes a gap between transactions */
#define CAM_I2C_READY_TRIALS        3u
#define CAM_I2C_READY_TIMEOUT_MS    20u

/** GPIO line test / bit-bang timing (half period in microseconds). */
#define CAM_I2C_RECOVER_PULSES      9u      /* I2C-bus spec §3.1.16 */
#define CAM_I2C_GPIO_HALF_US        20u     /* line test edges */
#define CAM_BB_HALF_US              10u     /* bit-bang SCCB ≈ 50 kHz */

#define CAM_I2C_LINE_SDA            0x01u
#define CAM_I2C_LINE_SCL            0x02u

/** Line-test result bits (see file header). */
#define CAM_LT_SDA_HIGH             0x01u
#define CAM_LT_SCL_HIGH             0x02u
#define CAM_LT_SDA_LOW              0x04u
#define CAM_LT_SCL_LOW              0x08u
#define CAM_LT_RELEASED             0x10u

static bool s_ok;
static bool s_spi_ok;
static bool s_i2c_ok;
static uint8_t s_rev;
static uint16_t s_pid;
static bool s_jpeg_ready;
static camera_res_t s_res;

/*
 * Bring-up diagnostics (F9 bench). External linkage and volatile so they
 * resolve in the debugger from any stop location; nothing in the firmware
 * reads them. Decode table in the file header.
 */
volatile uint32_t g_cam_spi_xfer_fail;
volatile uint8_t g_cam_spi_rd_a;
volatile uint8_t g_cam_spi_rd_b;
volatile uint8_t g_cam_arduchip_rev;
volatile uint8_t g_cam_spi_fail_stage = (uint8_t)CAMERA_STAGE_NOT_RUN;
volatile uint8_t g_cam_i2c_lines_idle;
volatile uint8_t g_cam_i2c_line_test;
volatile uint8_t g_cam_i2c_line_test2;   /* second run, before the filter-off retry */
volatile uint16_t g_cam_i2c_drive_dbg;   /* pin registers captured while both lines are driven low */
volatile uint32_t g_cam_i2c_recoveries;
volatile uint16_t g_cam_i2c_pin_cfg;
volatile uint8_t g_cam_i2c_attempt;
volatile uint8_t g_cam_i2c_ack;
volatile uint8_t g_cam_i2c_hal_status;
volatile uint32_t g_cam_i2c_hal_err;
volatile uint16_t g_cam_i2c_cr1;
volatile uint16_t g_cam_i2c_sr1;
volatile uint16_t g_cam_i2c_sr2;
volatile uint8_t g_cam_bb_ack;
volatile uint8_t g_cam_bb_pidh;
volatile uint8_t g_cam_bb_pidl;
volatile uint8_t g_cam_pidh;
volatile uint8_t g_cam_pidl;
volatile uint8_t g_cam_midh;
volatile uint8_t g_cam_midl;
volatile uint8_t g_cam_mid_ok;   /* MID matched 0x7FA2 (informational; PID gates) */
volatile uint8_t g_cam_i2c_fail_stage = (uint8_t)CAMERA_STAGE_NOT_RUN;
volatile uint32_t g_cam_last_fifo_len;
volatile uint32_t g_cam_capture_ok;
volatile uint32_t g_cam_capture_fail;
volatile uint8_t g_cam_soi_ok;   /* last capture began with JPEG SOI 0xFFD8 */
volatile uint8_t g_cam_eoi_ok;   /* last capture ended with JPEG EOI 0xFFD9 (0 if truncated) */

static void camera_set_ok(bool ok)
{
  s_ok = ok;
  error_flags_set_cam_ok(ok);
}

/* ---- Microsecond delay (DWT cycle counter, Cortex-M4) ------------------- */

static void cam_dwt_enable(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0u;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void cam_delay_us(uint32_t us)
{
  const uint32_t start = DWT->CYCCNT;
  const uint32_t ticks = us * (SystemCoreClock / 1000000u);
  uint32_t guard = (ticks * 2u) + 16u;   /* bounded even if CYCCNT ever stalls */

  while (((DWT->CYCCNT - start) < ticks) && (guard != 0u))
  {
    guard--;
  }
}

/* ---- ArduChip over SPI1 -------------------------------------------------- */

static bool cam_spi_write_reg(uint8_t reg, uint8_t value)
{
  uint8_t tx[CAM_REG_TRANSFER_BYTES];

  tx[0] = (uint8_t)(reg | CAM_SPI_WRITE_BIT);
  tx[1] = value;

  if (!spi_bus_transfer(Cam_CS_GPIO_Port, Cam_CS_Pin, tx, NULL,
                        CAM_REG_TRANSFER_BYTES, CAM_SPI_TIMEOUT_MS))
  {
    g_cam_spi_xfer_fail++;
    return false;
  }

  return true;
}

static bool cam_spi_read_reg(uint8_t reg, uint8_t *value)
{
  uint8_t tx[CAM_REG_TRANSFER_BYTES];
  uint8_t rx[CAM_REG_TRANSFER_BYTES];

  if (value == NULL)
  {
    return false;
  }

  tx[0] = (uint8_t)(reg & (uint8_t)~CAM_SPI_WRITE_BIT);
  tx[1] = 0u;

  if (!spi_bus_transfer(Cam_CS_GPIO_Port, Cam_CS_Pin, tx, rx,
                        CAM_REG_TRANSFER_BYTES, CAM_SPI_TIMEOUT_MS))
  {
    g_cam_spi_xfer_fail++;
    return false;
  }

  *value = rx[1];
  return true;
}

#if CAMERA_CPLD_RESET_ON_INIT
static void cam_cpld_reset(void)
{
  (void)cam_spi_write_reg(CAM_REG_CPLD, CAM_CPLD_RESET_BIT);
  HAL_Delay(CAM_CPLD_RESET_SETTLE_MS);
  (void)cam_spi_write_reg(CAM_REG_CPLD, 0u);
  HAL_Delay(CAM_CPLD_RESET_SETTLE_MS);
}
#endif

/*
 * Known-answer test: write a pattern to the TEST1 scratch register and read it
 * back. The read clocks 0x00 out on MOSI, so a MOSI–MISO short echoes 0x00 and
 * a floating MISO reads 0x00/0xFF — neither can pass by accident.
 */
static bool cam_spi_test_pattern(uint8_t pattern, volatile uint8_t *readback)
{
  uint8_t rb = 0u;

  if (!cam_spi_write_reg(CAM_REG_TEST1, pattern))
  {
    return false;
  }

  if (!cam_spi_read_reg(CAM_REG_TEST1, &rb))
  {
    return false;
  }

  *readback = rb;
  return rb == pattern;
}

static bool camera_probe_spi(void)
{
  uint8_t rev = 0u;

#if CAMERA_CPLD_RESET_ON_INIT
  cam_cpld_reset();
#endif

  if (!cam_spi_test_pattern(CAM_TEST_PATTERN_A, &g_cam_spi_rd_a))
  {
    g_cam_spi_fail_stage = (uint8_t)CAMERA_STAGE_SPI_TEST_A;
    return false;
  }

  if (!cam_spi_test_pattern(CAM_TEST_PATTERN_B, &g_cam_spi_rd_b))
  {
    g_cam_spi_fail_stage = (uint8_t)CAMERA_STAGE_SPI_TEST_B;
    return false;
  }

  if (!cam_spi_read_reg(CAM_REG_REV, &rev))
  {
    g_cam_spi_fail_stage = (uint8_t)CAMERA_STAGE_SPI_REV;
    return false;
  }

  s_rev = rev;
  g_cam_arduchip_rev = rev;
  g_cam_spi_fail_stage = (uint8_t)CAMERA_STAGE_PASS;
  return true;
}

/* ---- I2C1 pins as GPIO: line test, recovery, bit-bang ------------------- */

static void cam_gpio_scl(uint8_t high)
{
  HAL_GPIO_WritePin(CAM_I2C_GPIO_PORT, CAM_I2C_SCL_PIN,
                    (high != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void cam_gpio_sda(uint8_t high)
{
  HAL_GPIO_WritePin(CAM_I2C_GPIO_PORT, CAM_I2C_SDA_PIN,
                    (high != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static uint8_t cam_gpio_scl_read(void)
{
  return (HAL_GPIO_ReadPin(CAM_I2C_GPIO_PORT, CAM_I2C_SCL_PIN) == GPIO_PIN_SET) ? 1u : 0u;
}

static uint8_t cam_gpio_sda_read(void)
{
  return (HAL_GPIO_ReadPin(CAM_I2C_GPIO_PORT, CAM_I2C_SDA_PIN) == GPIO_PIN_SET) ? 1u : 0u;
}

static uint8_t cam_i2c_sample_lines(void)
{
  uint8_t lines = 0u;

  /* IDR reflects the pad level whatever mode the pin is in. */
  if (cam_gpio_scl_read() != 0u)
  {
    lines |= CAM_I2C_LINE_SCL;
  }

  if (cam_gpio_sda_read() != 0u)
  {
    lines |= CAM_I2C_LINE_SDA;
  }

  return lines;
}

/*
 * Hand PB6/PB7 to software: HAL_I2C_DeInit runs the CubeMX MspDeInit (pins
 * released, I2C1 clock off, handle back to RESET state — which also clears a
 * handle left locked/BUSY by a timed-out HAL call), then both pins become
 * open-drain outputs, released high before the driver is enabled.
 */
static void cam_i2c_pins_to_gpio(void)
{
  GPIO_InitTypeDef gpio = {0};

  (void)HAL_I2C_DeInit(&hi2c1);

  HAL_GPIO_WritePin(CAM_I2C_GPIO_PORT, CAM_I2C_SCL_PIN | CAM_I2C_SDA_PIN, GPIO_PIN_SET);
  gpio.Pin = CAM_I2C_SCL_PIN | CAM_I2C_SDA_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CAM_I2C_GPIO_PORT, &gpio);
}

static void cam_i2c_note_failure(HAL_StatusTypeDef status)
{
  g_cam_i2c_hal_status = (uint8_t)status;
  g_cam_i2c_hal_err = hi2c1.ErrorCode;
}

/*
 * Hand PB6/PB7 back to I2C1: HAL_I2C_Init on a RESET-state handle re-runs the
 * CubeMX MspInit (AF4 open-drain, I2C1 clock), pulses SWRST, then programs
 * FREQ/CCR/TRISE from hi2c1.Init — ClockSpeed was set to CAMERA_I2C_CLOCK_HZ
 * at the top of the probe, so every re-init lands at the bring-up rate.
 */
static bool cam_i2c_pins_to_periph(void)
{
  HAL_StatusTypeDef status;

  status = HAL_I2C_Init(&hi2c1);
  if (status != HAL_OK)
  {
    cam_i2c_note_failure(status);
    return false;
  }

  return true;
}

/*
 * ST errata recovery sequence with readback at every step (pins must be GPIO):
 * release both, clock SDA free if a slave holds it, prove each line can be
 * driven low, then a STOP. Doubles as the "can the MCU pull the wire low"
 * test that a HAL_I2C_WRONG_START failure calls for.
 */
static uint8_t cam_i2c_line_test(void)
{
  uint8_t result = 0u;
  uint32_t i;

  cam_gpio_sda(1u);
  cam_gpio_scl(1u);
  cam_delay_us(CAM_I2C_GPIO_HALF_US);

  if (cam_gpio_sda_read() != 0u)
  {
    result |= CAM_LT_SDA_HIGH;
  }
  else
  {
    g_cam_i2c_recoveries++;
    for (i = 0u; i < CAM_I2C_RECOVER_PULSES; i++)
    {
      cam_gpio_scl(0u);
      cam_delay_us(CAM_I2C_GPIO_HALF_US);
      cam_gpio_scl(1u);
      cam_delay_us(CAM_I2C_GPIO_HALF_US);
      if (cam_gpio_sda_read() != 0u)
      {
        break;
      }
    }
  }

  if (cam_gpio_scl_read() != 0u)
  {
    result |= CAM_LT_SCL_HIGH;
  }

  cam_gpio_sda(0u);
  cam_delay_us(CAM_I2C_GPIO_HALF_US);
  if (cam_gpio_sda_read() == 0u)
  {
    result |= CAM_LT_SDA_LOW;
  }

  cam_gpio_scl(0u);
  cam_delay_us(CAM_I2C_GPIO_HALF_US);
  if (cam_gpio_scl_read() == 0u)
  {
    result |= CAM_LT_SCL_LOW;
  }

  /* Both lines driven low right now: snapshot what the pad, ODR, MODER and
   * OTYPER say. IDR high with ODR low, MODER 01 and OTYPER 1 means the pin
   * driver is on and the wire is still being held high externally. */
  g_cam_i2c_drive_dbg = (uint16_t)(
      ((CAM_I2C_GPIO_PORT->IDR >> CAM_I2C_SCL_BIT) & 0x3u) |
      (((CAM_I2C_GPIO_PORT->ODR >> CAM_I2C_SCL_BIT) & 0x3u) << 4) |
      (((CAM_I2C_GPIO_PORT->MODER >> (2u * CAM_I2C_SCL_BIT)) & 0xFu) << 8) |
      (((CAM_I2C_GPIO_PORT->OTYPER >> CAM_I2C_SCL_BIT) & 0x3u) << 12));

  /* STOP: SCL high, then SDA low → high. */
  cam_gpio_scl(1u);
  cam_delay_us(CAM_I2C_GPIO_HALF_US);
  cam_gpio_sda(1u);
  cam_delay_us(CAM_I2C_GPIO_HALF_US);

  if ((cam_gpio_sda_read() != 0u) && (cam_gpio_scl_read() != 0u))
  {
    result |= CAM_LT_RELEASED;
  }

  return result;
}

/** Packed PB6/PB7 configuration: (AF7 << 12) | (AF6 << 8) | (OTYPER[7:6] << 4) | MODER[7:6]. */
static uint16_t cam_i2c_pin_cfg(void)
{
  const uint32_t moder = (CAM_I2C_GPIO_PORT->MODER >> (2u * CAM_I2C_SCL_BIT)) & 0xFu;
  const uint32_t otype = (CAM_I2C_GPIO_PORT->OTYPER >> CAM_I2C_SCL_BIT) & 0x3u;
  const uint32_t af6 = (CAM_I2C_GPIO_PORT->AFR[0] >> (4u * CAM_I2C_SCL_BIT)) & 0xFu;
  const uint32_t af7 = (CAM_I2C_GPIO_PORT->AFR[0] >> (4u * CAM_I2C_SDA_BIT)) & 0xFu;

  return (uint16_t)((af7 << 12) | (af6 << 8) | (otype << 4) | moder);
}

static void cam_i2c_capture_regs(void)
{
  g_cam_i2c_cr1 = (uint16_t)hi2c1.Instance->CR1;
  g_cam_i2c_sr1 = (uint16_t)hi2c1.Instance->SR1;
  g_cam_i2c_sr2 = (uint16_t)hi2c1.Instance->SR2;
}

/* Bit-banged SCCB (I2C-compatible) on the same pins — no peripheral involved. */

static void cam_bb_start(void)
{
  cam_gpio_sda(1u);
  cam_gpio_scl(1u);
  cam_delay_us(CAM_BB_HALF_US);
  cam_gpio_sda(0u);
  cam_delay_us(CAM_BB_HALF_US);
  cam_gpio_scl(0u);
  cam_delay_us(CAM_BB_HALF_US);
}

static void cam_bb_stop(void)
{
  cam_gpio_sda(0u);
  cam_delay_us(CAM_BB_HALF_US);
  cam_gpio_scl(1u);
  cam_delay_us(CAM_BB_HALF_US);
  cam_gpio_sda(1u);
  cam_delay_us(CAM_BB_HALF_US);
}

/* Returns true if the slave pulled SDA low on the ninth clock (ACK). */
static bool cam_bb_write_byte(uint8_t value)
{
  uint32_t i;
  bool ack;

  for (i = 0u; i < 8u; i++)
  {
    cam_gpio_sda(((value & 0x80u) != 0u) ? 1u : 0u);
    value = (uint8_t)(value << 1);
    cam_delay_us(CAM_BB_HALF_US);
    cam_gpio_scl(1u);
    cam_delay_us(CAM_BB_HALF_US);
    cam_gpio_scl(0u);
    cam_delay_us(CAM_BB_HALF_US);
  }

  cam_gpio_sda(1u);                      /* release for the ACK bit */
  cam_delay_us(CAM_BB_HALF_US);
  cam_gpio_scl(1u);
  cam_delay_us(CAM_BB_HALF_US);
  ack = (cam_gpio_sda_read() == 0u);
  cam_gpio_scl(0u);
  cam_delay_us(CAM_BB_HALF_US);

  return ack;
}

/* Single-byte read: master NACKs after the byte. */
static uint8_t cam_bb_read_byte(void)
{
  uint32_t i;
  uint8_t value = 0u;

  cam_gpio_sda(1u);
  for (i = 0u; i < 8u; i++)
  {
    cam_gpio_scl(1u);
    cam_delay_us(CAM_BB_HALF_US);
    value = (uint8_t)((value << 1) | cam_gpio_sda_read());
    cam_gpio_scl(0u);
    cam_delay_us(CAM_BB_HALF_US);
  }

  cam_gpio_sda(1u);                      /* NACK */
  cam_delay_us(CAM_BB_HALF_US);
  cam_gpio_scl(1u);
  cam_delay_us(CAM_BB_HALF_US);
  cam_gpio_scl(0u);
  cam_delay_us(CAM_BB_HALF_US);

  return value;
}

static bool cam_bb_sccb_write(uint8_t reg, uint8_t value)
{
  bool ok;

  cam_bb_start();
  ok = cam_bb_write_byte(CAM_OV2640_ADDR_W);
  ok = ok && cam_bb_write_byte(reg);
  ok = ok && cam_bb_write_byte(value);
  cam_bb_stop();

  return ok;
}

static bool cam_bb_sccb_read(uint8_t reg, uint8_t *value)
{
  bool ok;

  cam_bb_start();
  ok = cam_bb_write_byte(CAM_OV2640_ADDR_W);
  ok = ok && cam_bb_write_byte(reg);
  cam_bb_stop();

  if (!ok)
  {
    return false;
  }

  cam_bb_start();
  ok = cam_bb_write_byte(CAM_OV2640_ADDR_R);
  if (ok)
  {
    *value = cam_bb_read_byte();
  }
  cam_bb_stop();

  return ok;
}

/* Pins must be GPIO. Answers "is there an OV2640 on these wires at all?". */
static void cam_bb_probe(void)
{
  uint8_t v = 0u;

  g_cam_bb_ack = cam_bb_sccb_write(CAM_OV2640_REG_BANK_SEL, CAM_OV2640_BANK_SENSOR) ? 1u : 0u;

  if (cam_bb_sccb_read(CAM_OV2640_REG_PIDH, &v))
  {
    g_cam_bb_pidh = v;
  }

  if (cam_bb_sccb_read(CAM_OV2640_REG_PIDL, &v))
  {
    g_cam_bb_pidl = v;
  }
}

/* ---- OV2640 over I2C1 (HAL) ---------------------------------------------- */

static bool cam_i2c_ready(void)
{
  HAL_StatusTypeDef status;

  status = HAL_I2C_IsDeviceReady(&hi2c1, CAMERA_OV2640_I2C_ADDR,
                                 CAM_I2C_READY_TRIALS, CAM_I2C_READY_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    cam_i2c_note_failure(status);
    return false;
  }

  return true;
}

static bool cam_sccb_write(uint8_t reg, uint8_t value)
{
  uint8_t tx[2];
  HAL_StatusTypeDef status;

  tx[0] = reg;
  tx[1] = value;

  status = HAL_I2C_Master_Transmit(&hi2c1, CAMERA_OV2640_I2C_ADDR, tx, 2u,
                                   CAM_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    cam_i2c_note_failure(status);
    return false;
  }

  return true;
}

/* SCCB read: write the register index, STOP, then a one-byte read. */
static bool cam_sccb_read(uint8_t reg, uint8_t *value)
{
  HAL_StatusTypeDef status;

  if (value == NULL)
  {
    return false;
  }

  HAL_Delay(CAM_SCCB_READ_SETTLE_MS);

  status = HAL_I2C_Master_Transmit(&hi2c1, CAMERA_OV2640_I2C_ADDR, &reg, 1u,
                                   CAM_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    cam_i2c_note_failure(status);
    return false;
  }

  status = HAL_I2C_Master_Receive(&hi2c1, CAMERA_OV2640_I2C_ADDR, value, 1u,
                                  CAM_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    cam_i2c_note_failure(status);
    return false;
  }

  return true;
}

/*
 * Get the OV2640 to ACK its address on the HAL path.
 *   Attempt 1: line test/recovery over GPIO, re-init, analog filter on (default).
 *   Attempt 2: same again with the analog filter off (STM32F4 I2C errata: the
 *              filter can lock the peripheral so no START is ever generated).
 *   Fallback:  bit-banged SCCB probe so the wiring/sensor can be judged even
 *              when the peripheral path is dead; pins are handed back after.
 */
static bool cam_i2c_acquire_sensor(void)
{
  cam_i2c_pins_to_gpio();
  g_cam_i2c_line_test = cam_i2c_line_test();
  if (!cam_i2c_pins_to_periph())
  {
    g_cam_i2c_fail_stage = (uint8_t)CAMERA_STAGE_I2C_CLOCK;
    return false;
  }
  g_cam_i2c_pin_cfg = cam_i2c_pin_cfg();

  if (cam_i2c_ready())
  {
    g_cam_i2c_attempt = 1u;
    return true;
  }

  cam_i2c_capture_regs();

  cam_i2c_pins_to_gpio();
  g_cam_i2c_line_test2 = cam_i2c_line_test();
  if (cam_i2c_pins_to_periph() &&
      (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_DISABLE) == HAL_OK) &&
      cam_i2c_ready())
  {
    g_cam_i2c_attempt = 2u;
    return true;
  }

  cam_i2c_pins_to_gpio();
  cam_bb_probe();
  (void)cam_i2c_pins_to_periph();

  g_cam_i2c_fail_stage = (uint8_t)CAMERA_STAGE_I2C_ACK;
  return false;
}

static bool camera_probe_i2c(void)
{
  uint8_t pidh = 0u;
  uint8_t pidl = 0u;
  uint8_t midh = 0u;
  uint8_t midl = 0u;

  cam_dwt_enable();
  hi2c1.Init.ClockSpeed = CAMERA_I2C_CLOCK_HZ;
  g_cam_i2c_lines_idle = cam_i2c_sample_lines();
  g_cam_i2c_ack = 0u;
  g_cam_i2c_attempt = 0u;

  if (!cam_i2c_acquire_sensor())
  {
    return false;
  }

  g_cam_i2c_ack = 1u;

  if (!cam_sccb_write(CAM_OV2640_REG_BANK_SEL, CAM_OV2640_BANK_SENSOR))
  {
    g_cam_i2c_fail_stage = (uint8_t)CAMERA_STAGE_I2C_BANK;
    return false;
  }

  if (!cam_sccb_read(CAM_OV2640_REG_PIDH, &pidh) ||
      !cam_sccb_read(CAM_OV2640_REG_PIDL, &pidl))
  {
    g_cam_i2c_fail_stage = (uint8_t)CAMERA_STAGE_I2C_PID;
    return false;
  }

  g_cam_pidh = pidh;
  g_cam_pidl = pidl;
  s_pid = (uint16_t)(((uint16_t)pidh << 8) | (uint16_t)pidl);

  if (!camera_ov2640_pid_ok(pidh, pidl))
  {
    g_cam_i2c_fail_stage = (uint8_t)CAMERA_STAGE_I2C_PID;
    return false;
  }

  /*
   * PID (0x26 0x41|0x42) is the definitive OV2640 identity and has already
   * passed by here. MID (OmniVision 0x7FA2) is a secondary confirmation; some
   * OV2640 modules read it back as 0xFF. Record it but do NOT gate the probe on
   * it — a correct PID means the sensor is present and the SCCB bus works.
   */
  if (cam_sccb_read(CAM_OV2640_REG_MIDH, &midh) &&
      cam_sccb_read(CAM_OV2640_REG_MIDL, &midl))
  {
    g_cam_midh = midh;
    g_cam_midl = midl;
    g_cam_mid_ok = camera_ov2640_mid_ok(midh, midl) ? 1u : 0u;
  }

  g_cam_i2c_fail_stage = (uint8_t)CAMERA_STAGE_PASS;
  return true;
}

/* ---- OV2640 JPEG config + ArduChip FIFO capture (F9.1b / F9.2) ------------ */

static bool cam_sccb_write_table(const cam_sensor_reg_t *t)
{
  uint32_t i;

  for (i = 0u; ; i++)
  {
    if (t[i].reg == CAM_REG_TABLE_END && t[i].val == CAM_REG_TABLE_END)
    {
      break;
    }
    if (!cam_sccb_write(t[i].reg, t[i].val))
    {
      return false;
    }
  }

  return true;
}

bool camera_configure_jpeg(camera_res_t res)
{
  const cam_sensor_reg_t *size_tbl =
      (res == CAMERA_RES_320x240) ? OV2640_320x240_JPEG : OV2640_160x120_JPEG;

  s_jpeg_ready = false;

  if (!s_i2c_ok)
  {
    return false;
  }

  /* Sensor bank, COM7 soft reset, settle. */
  if (!cam_sccb_write(CAM_OV2640_REG_BANK_SEL, CAM_OV2640_BANK_SENSOR) ||
      !cam_sccb_write(CAM_OV2640_REG_COM7, CAM_OV2640_COM7_SRST))
  {
    return false;
  }
  HAL_Delay(100u);

  if (!cam_sccb_write_table(OV2640_JPEG_INIT) ||
      !cam_sccb_write_table(OV2640_YUV422) ||
      !cam_sccb_write_table(OV2640_JPEG))
  {
    return false;
  }

  /* Back to sensor bank, clear COM10-ish reserved, then resolution table. */
  if (!cam_sccb_write(CAM_OV2640_REG_BANK_SEL, CAM_OV2640_BANK_SENSOR) ||
      !cam_sccb_write(0x15u, 0x00u))
  {
    return false;
  }

  if (!cam_sccb_write_table(size_tbl))
  {
    return false;
  }

  HAL_Delay(CAM_JPEG_SETTLE_MS);
  s_res = res;
  s_jpeg_ready = true;
  return true;
}

static uint32_t cam_fifo_length(void)
{
  uint8_t s1 = 0u;
  uint8_t s2 = 0u;
  uint8_t s3 = 0u;

  (void)cam_spi_read_reg(CAM_REG_FIFO_SIZE1, &s1);
  (void)cam_spi_read_reg(CAM_REG_FIFO_SIZE2, &s2);
  (void)cam_spi_read_reg(CAM_REG_FIFO_SIZE3, &s3);

  return (((uint32_t)(s3 & 0x7Fu) << 16) | ((uint32_t)s2 << 8) | (uint32_t)s1) &
         CAM_FIFO_LEN_MASK;
}

/*
 * Burst-read n bytes from the ArduChip FIFO. Uses spi_bus_acquire so CS stays
 * asserted across the whole stream (the burst auto-increments the read pointer);
 * transfers pass cs_port NULL per the spi_bus acquired-window contract.
 */
static bool cam_fifo_burst_read(uint8_t *buf, uint32_t n)
{
  uint8_t cmd = CAM_REG_BURST_FIFO_READ;
  uint32_t off = 0u;
  bool ok;

  if (!spi_bus_acquire())
  {
    return false;
  }

  HAL_GPIO_WritePin(Cam_CS_GPIO_Port, Cam_CS_Pin, GPIO_PIN_RESET);

  ok = spi_bus_transfer(NULL, 0u, &cmd, NULL, 1u, CAM_SPI_TIMEOUT_MS);

  while (ok && (off < n))
  {
    uint32_t blk = n - off;
    if (blk > CAM_FIFO_BLOCK)
    {
      blk = CAM_FIFO_BLOCK;
    }
    ok = spi_bus_transfer(NULL, 0u, NULL, buf + off, (uint16_t)blk, CAM_SPI_TIMEOUT_MS);
    off += blk;
  }

  HAL_GPIO_WritePin(Cam_CS_GPIO_Port, Cam_CS_Pin, GPIO_PIN_SET);
  (void)spi_bus_release();
  return ok;
}

bool camera_capture(uint8_t *buf, uint32_t buf_max, uint32_t *out_len)
{
  uint32_t start;
  uint32_t len;
  uint32_t n;
  uint8_t trig = 0u;

  if (buf == NULL || out_len == NULL || buf_max == 0u)
  {
    return false;
  }
  *out_len = 0u;

  if (!cam_spi_write_reg(CAM_REG_FIFO, CAM_FIFO_CLEAR) ||
      !cam_spi_write_reg(CAM_REG_FIFO, CAM_FIFO_START))
  {
    g_cam_capture_fail++;
    return false;
  }

  start = HAL_GetTick();
  do
  {
    if (!cam_spi_read_reg(CAM_REG_TRIG, &trig))
    {
      g_cam_capture_fail++;
      return false;
    }
  } while (((trig & CAM_TRIG_CAP_DONE) == 0u) &&
           ((HAL_GetTick() - start) < CAM_CAPTURE_TIMEOUT_MS));

  if ((trig & CAM_TRIG_CAP_DONE) == 0u)
  {
    g_cam_capture_fail++;
    return false;
  }

  len = cam_fifo_length();
  g_cam_last_fifo_len = len;
  if (len == 0u)
  {
    g_cam_capture_fail++;
    return false;
  }

  n = (len > buf_max) ? buf_max : len;
  if (!cam_fifo_burst_read(buf, n))
  {
    g_cam_capture_fail++;
    return false;
  }

  /* Capture-verify: a valid JPEG opens with SOI 0xFFD8 and closes with EOI
     0xFFD9. EOI is only present when the frame fit in buf (n == len); a
     truncated frame (len > buf_max) leaves g_cam_eoi_ok 0, which is expected. */
  g_cam_soi_ok = ((n >= 2u) && (buf[0] == 0xFFu) && (buf[1] == 0xD8u)) ? 1u : 0u;
  g_cam_eoi_ok = ((n >= 2u) && (buf[n - 2u] == 0xFFu) && (buf[n - 1u] == 0xD9u)) ? 1u : 0u;

  *out_len = n;
  g_cam_capture_ok++;
  return true;
}

uint32_t camera_get_last_fifo_len(void)
{
  return g_cam_last_fifo_len;
}

bool camera_jpeg_ready(void)
{
  return s_jpeg_ready;
}

camera_res_t camera_get_res(void)
{
  return s_res;
}

/* ---- Public API ----------------------------------------------------------- */

bool camera_init(void)
{
  /* Probe both buses regardless of each other's result: one boot, both answers. */
  s_spi_ok = camera_probe_spi();
  s_i2c_ok = camera_probe_i2c();

  camera_set_ok(s_spi_ok && s_i2c_ok);
  return s_ok;
}

bool camera_is_ok(void)
{
  return s_ok;
}

bool camera_spi_ok(void)
{
  return s_spi_ok;
}

bool camera_i2c_ok(void)
{
  return s_i2c_ok;
}

uint8_t camera_get_arduchip_rev(void)
{
  return s_rev;
}

uint16_t camera_get_sensor_pid(void)
{
  return s_pid;
}

camera_stage_t camera_get_spi_fail_stage(void)
{
  return (camera_stage_t)g_cam_spi_fail_stage;
}

camera_stage_t camera_get_i2c_fail_stage(void)
{
  return (camera_stage_t)g_cam_i2c_fail_stage;
}

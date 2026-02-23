/*
 * Copyright (c) 2024 Kelvin
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#define EPD_node DT_NODELABEL(e_paper)

/* JD79661 Commands (Derived from common e-paper drivers) */
#define JD79661_CMD_PSR 0x00  // Panel Setting
#define JD79661_CMD_PWR 0x01  // Power Setting
#define JD79661_CMD_POF 0x02  // Power OFF
#define JD79661_CMD_PFS 0x03  // Power Off Sequence Setting
#define JD79661_CMD_PON 0x04  // Power ON
#define JD79661_CMD_BTST 0x06 // Booster Soft Start
#define JD79661_CMD_DSLP 0x07 // Deep Sleep
#define JD79661_CMD_DTM1 0x10 // Data Start Transmission 1
#define JD79661_CMD_DSP 0x11  // Data Stop
#define JD79661_CMD_DRF 0x12  // Display Refresh
#define JD79661_CMD_IPC 0x13  // Image Process Command
#define JD79661_CMD_PLL 0x30  // PLL Control
#define JD79661_CMD_TSC 0x40  // Temperature Sensor Command
#define JD79661_CMD_TSE 0x41  // Temperature Sensor Extension
#define JD79661_CMD_TSR 0x43  // Temperature Sensor Write
#define JD79661_CMD_CDI 0x50  // VCOM and Data Interval Setting
#define JD79661_CMD_LPD 0x51  // Low Power Detection
#define JD79661_CMD_EUS 0x52  // End Option Setting
#define JD79661_CMD_TCON 0x60 // TCON Setting
#define JD79661_CMD_TRES 0x61 // Resolution Setting
#define JD79661_CMD_REV 0x70  // Revision

/* Display Resolution */
#define EPD_WIDTH 200
#define EPD_HEIGHT 200

static const struct gpio_dt_spec busy_gpio =
    GPIO_DT_SPEC_GET_BY_IDX(EPD_node, busy_gpios, 0);
static const struct mipi_dbi_config dbi_config =
    MIPI_DBI_CONFIG_DT(EPD_node, SPI_OP_MODE_MASTER | SPI_WORD_SET(8), 0);

/* Helper wrapper to send commands using the MIPI DBI API */
static int jd79661_write_cmd(const struct device *dev, uint8_t cmd,
                             const uint8_t *data, size_t len) {
  return mipi_dbi_command_write(dev, &dbi_config, cmd, data, len);
}

static void epd_wait_busy(void) {
  int cnt = 0;
  /* Wait for BUSY to go LOW (Ready) or HIGH (Busy) - Depends on controller.
   * Providing standard check: High = Busy usually for SSD16xx/JD7966x
   */
  LOG_INF("Waiting for busy...");
  while (gpio_pin_get_dt(&busy_gpio) == 1) {
    k_msleep(10);
    cnt++;
    if (cnt > 2000) { // 20s timeout
      LOG_ERR("Busy timeout!");
      break;
    }
  }
  LOG_INF("Busy released");
}

static void epd_reset(const struct device *dev) {
  mipi_dbi_reset(dev, 20);
  k_msleep(20);
}

static int jd79661_init(const struct device *dev) {
  LOG_INF("Initializing JD79661...");

  epd_reset(dev);
  epd_wait_busy();

  /* Initialization Sequence (Matched to GDEY029F52 Example) */

  uint8_t cmd_4d[] = {0x78};
  jd79661_write_cmd(dev, 0x4D, cmd_4d, sizeof(cmd_4d));

  // 1. Panel Setting (PSR)
  uint8_t psr[] = {0x0f, 0x29};
  jd79661_write_cmd(dev, JD79661_CMD_PSR, psr, sizeof(psr));

  // 2. Power Setting (PWR)
  uint8_t pwr[] = {0x07, 0x00};
  jd79661_write_cmd(dev, JD79661_CMD_PWR, pwr, sizeof(pwr));

  // 3. Power Off Sequence (POFS)
  uint8_t pofs[] = {0x10, 0x54, 0x44};
  jd79661_write_cmd(dev, JD79661_CMD_PFS, pofs, sizeof(pofs));

  // 4. Booster Soft Start (BTST)
  uint8_t btst[] = {0x0f, 0x0a, 0x2f, 0x25, 0x22, 0x2e, 0x21};
  jd79661_write_cmd(dev, JD79661_CMD_BTST, btst, sizeof(btst));

  // Temperature Sensor (TSE)
  uint8_t tse[] = {0x00};
  jd79661_write_cmd(dev, JD79661_CMD_TSE, tse, sizeof(tse));

  // 5. VCOM and Data Interval Setting (CDI)
  uint8_t cdi[] = {0x37};
  jd79661_write_cmd(dev, JD79661_CMD_CDI, cdi, sizeof(cdi));

  // 6. TCON Setting
  uint8_t tcon[] = {0x02, 0x02};
  jd79661_write_cmd(dev, JD79661_CMD_TCON, tcon, sizeof(tcon));

  // 7. Resolution Setting
  uint8_t tres[] = {EPD_WIDTH / 256, EPD_WIDTH % 256, EPD_HEIGHT / 256,
                    EPD_HEIGHT % 256};
  jd79661_write_cmd(dev, JD79661_CMD_TRES, tres, sizeof(tres));

  // GSST
  uint8_t gsst[] = {0x00, 0x00, 0x00, 0x00};
  jd79661_write_cmd(dev, 0x65, gsst, sizeof(gsst));

  // 8. Other Vendor Specific Commands
  uint8_t cmd_e7[] = {0x1c};
  jd79661_write_cmd(dev, 0xe7, cmd_e7, sizeof(cmd_e7));
  uint8_t cmd_e3[] = {0x22};
  jd79661_write_cmd(dev, 0xe3, cmd_e3, sizeof(cmd_e3));
  uint8_t cmd_b4[] = {0xd0};
  jd79661_write_cmd(dev, 0xb4, cmd_b4, sizeof(cmd_b4));
  uint8_t cmd_b5[] = {0x03};
  jd79661_write_cmd(dev, 0xb5, cmd_b5, sizeof(cmd_b5));
  uint8_t cmd_e9[] = {0x01};
  jd79661_write_cmd(dev, 0xe9, cmd_e9, sizeof(cmd_e9));

  // 9. PLL Control
  uint8_t pll[] = {0x08};
  jd79661_write_cmd(dev, JD79661_CMD_PLL, pll, sizeof(pll));

  // 10. Power ON
  jd79661_write_cmd(dev, JD79661_CMD_PON, NULL, 0);
  epd_wait_busy();

  LOG_INF("JD79661 Initialized");
  return 0;
}

static void jd79661_display_frame(const struct device *dev,
                                  const uint8_t *buffer, size_t size) {
  LOG_INF("Sending framebuffer...");

  // Start Data Transmission
  jd79661_write_cmd(dev, JD79661_CMD_DTM1, buffer, size);

  LOG_INF("Refreshing display...");
  // Display Refresh
  jd79661_write_cmd(dev, JD79661_CMD_DRF, NULL, 0);
  epd_wait_busy();

  LOG_INF("Display update complete");
}

static void jd79661_deep_sleep(const struct device *dev) {
  uint8_t dslp[] = {0xa5};
  jd79661_write_cmd(dev, JD79661_CMD_POF, NULL, 0); // Power OFF
  epd_wait_busy();
  jd79661_write_cmd(dev, JD79661_CMD_DSLP, dslp, sizeof(dslp)); // Deep Sleep
}

/* LED Support */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Buffer for 200x200 4-color display (2 bits per pixel usually, packed)
 * But standard SPI EPDs often take 1 byte per 2 pixels or strange formats.
 * For JD79661 (4-color), it might be similar to SSD1681 where different buffers
 * are used or packed. Assuming simpler case: Init pattern.
 */
#define BUFFER_SIZE                                                            \
  (EPD_WIDTH * EPD_HEIGHT /                                                    \
   2) // 2 pixels per byte? Or 1 bit per pixel * planes?
// Let's create a pattern buffer.
static uint8_t frame_buffer[BUFFER_SIZE];

int main(void) {
  int ret;
  const struct device *mipi_dev;

  LOG_INF("Starting JD79661 Manual Driver Example");

  /* LED Init */
  if (!gpio_is_ready_dt(&led)) {
    LOG_ERR("LED GPIO not ready");
    return -ENODEV;
  }
  gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

  /* MIPI DBI Device Init */
  // We use the parent mipi-dbi-spi device node to get the bus controller
  // But we need the specific device instance.
  // Since we defined `e_paper` as child of `mipi_dbi_epaper`,
  // `DEVICE_DT_GET(DT_NODELABEL(mipi_dbi_epaper))` gives the MIPI DBI
  // controller instance? Actually, in Zephyr `zephyr,mipi-dbi-spi` acts as the
  // controller. We need to pass that device to `mipi_dbi_command_write`.

  mipi_dev = DEVICE_DT_GET(DT_NODELABEL(mipi_dbi_epaper));
  if (!device_is_ready(mipi_dev)) {
    LOG_ERR("MIPI DBI device not ready");
    return -ENODEV;
  }

  /* BUSY GPIO Init */
  if (!gpio_is_ready_dt(&busy_gpio)) {
    LOG_ERR("BUSY GPIO not ready");
    return -ENODEV;
  }
  gpio_pin_configure_dt(&busy_gpio, GPIO_INPUT);

  /* Initialize Display */
  ret = jd79661_init(mipi_dev);
  if (ret < 0) {
    LOG_ERR("Failed to init display");
    return ret;
  }

  /* Fill buffer with solid Yellow */
  /* For JD79661 (2 bits per pixel):
   * 0x00 = Black (00 00 00 00)
   * 0x55 = White (01 01 01 01)
   * 0xAA = Yellow (10 10 10 10)
   * 0xFF = Red (11 11 11 11)
   */
  for (int i = 0; i < BUFFER_SIZE; i++) {
    frame_buffer[i] = 0xAA; // Solid Yellow
  }

  while (1) {
    gpio_pin_toggle_dt(&led);

    LOG_INF("Updating display...");
    jd79661_display_frame(mipi_dev, frame_buffer, sizeof(frame_buffer));

    // Wait before next update
    k_sleep(K_SECONDS(30));

    // Re-init if deep sleep was used (not currently enabled to keep loop
    // simple)
  }
}

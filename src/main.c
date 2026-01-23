#include <string.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define EPD_NODE DT_NODELABEL(e_paper)
#define EPD_WIDTH DT_PROP(EPD_NODE, width)
#define EPD_HEIGHT DT_PROP(EPD_NODE, height)
#define EPD_BUF_SIZE (EPD_WIDTH * EPD_HEIGHT / 8U)
#define EPD_PANEL_ROWS_PER_PAGE 8U

#define SSD16XX_CMD_RAM_XPOS_CTRL 0x44
#define SSD16XX_CMD_RAM_YPOS_CTRL 0x45
#define SSD16XX_CMD_RAM_XPOS_CNTR 0x4e
#define SSD16XX_CMD_RAM_YPOS_CNTR 0x4f
#define SSD16XX_CMD_WRITE_RED_RAM 0x26

static const struct mipi_dbi_config epd_dbi_config =
	MIPI_DBI_CONFIG_DT(EPD_NODE,
			   SPI_OP_MODE_MASTER | SPI_WORD_SET(8) |
				   SPI_HOLD_ON_CS | SPI_LOCK_ON,
			   0);

static const struct gpio_dt_spec epd_busy = GPIO_DT_SPEC_GET(EPD_NODE, busy_gpios);

static void epd_busy_wait(void)
{
	int pin = gpio_pin_get_dt(&epd_busy);

	while (pin > 0) {
		k_msleep(1);
		pin = gpio_pin_get_dt(&epd_busy);
	}
}

static void epd_set_mono_pixel(uint8_t *buf, uint16_t x, uint16_t y,
			       uint16_t height, bool on, bool msb_first)
{
	const uint16_t bytes_per_col = height / EPD_PANEL_ROWS_PER_PAGE;
	const uint32_t index = (x * bytes_per_col) + (y / EPD_PANEL_ROWS_PER_PAGE);
	const uint8_t bit = y % EPD_PANEL_ROWS_PER_PAGE;
	const uint8_t mask = (uint8_t)(1U << (msb_first ? (7U - bit) : bit));

	if (on) {
		buf[index] |= mask;
	} else {
		buf[index] &= (uint8_t)~mask;
	}
}

static int epd_set_window(const struct device *dbi_dev, uint16_t x, uint16_t y,
			  uint16_t width, uint16_t height)
{
	const uint16_t panel_h = EPD_HEIGHT - (EPD_HEIGHT % EPD_PANEL_ROWS_PER_PAGE);
	const uint16_t x_start = (panel_h - 1U - y) / EPD_PANEL_ROWS_PER_PAGE;
	const uint16_t x_end =
		(panel_h - 1U - (y + height - 1U)) / EPD_PANEL_ROWS_PER_PAGE;
	const uint16_t y_start = x;
	const uint16_t y_end = (uint16_t)(x + width - 1U);
	uint8_t tmp[4];
	int err;

	tmp[0] = (uint8_t)x_start;
	tmp[1] = (uint8_t)x_end;
	err = mipi_dbi_command_write(dbi_dev, &epd_dbi_config,
				     SSD16XX_CMD_RAM_XPOS_CTRL, tmp, 2);
	if (err < 0) {
		return err;
	}

	sys_put_le16(y_start, tmp);
	sys_put_le16(y_end, tmp + 2);
	err = mipi_dbi_command_write(dbi_dev, &epd_dbi_config,
				     SSD16XX_CMD_RAM_YPOS_CTRL, tmp, 4);
	if (err < 0) {
		return err;
	}

	tmp[0] = (uint8_t)x_start;
	err = mipi_dbi_command_write(dbi_dev, &epd_dbi_config,
				     SSD16XX_CMD_RAM_XPOS_CNTR, tmp, 1);
	if (err < 0) {
		return err;
	}

	sys_put_le16(y_start, tmp);
	return mipi_dbi_command_write(dbi_dev, &epd_dbi_config,
				      SSD16XX_CMD_RAM_YPOS_CNTR, tmp, 2);
}

static int epd_write_red_ram(const struct device *dbi_dev, const uint8_t *buf,
			     uint16_t width, uint16_t height)
{
	struct display_buffer_descriptor desc = {
		.width = width,
		.height = height,
		.pitch = width,
		.buf_size = width * height / 8U,
	};
	int err;

	err = epd_set_window(dbi_dev, 0, 0, width, height);
	if (err < 0) {
		return err;
	}

	err = mipi_dbi_command_write(dbi_dev, &epd_dbi_config,
				     SSD16XX_CMD_WRITE_RED_RAM, buf,
				     desc.buf_size);
	if (err < 0) {
		return err;
	}

	mipi_dbi_release(dbi_dev, &epd_dbi_config);
	return 0;
}

static void epd_fill_pattern(uint8_t *black_buf, uint8_t *color_buf,
			     uint16_t width, uint16_t height, bool msb_first)
{
	const uint16_t half_w = width / 2U;
	const uint16_t half_h = height / 2U;

	memset(black_buf, 0, EPD_BUF_SIZE);
	memset(color_buf, 0, EPD_BUF_SIZE);

	for (uint16_t y = 0; y < height; y++) {
		for (uint16_t x = 0; x < width; x++) {
			const bool left = x < half_w;
			const bool top = y < half_h;
			const bool black = !left && top;
			const bool red = left && !top;
			const bool yellow = !left && !top;

			if (black || yellow) {
				epd_set_mono_pixel(black_buf, x, y, height, true,
						   msb_first);
			}

			if (red || yellow) {
				epd_set_mono_pixel(color_buf, x, y, height, true,
						   msb_first);
			}
		}
	}
}

static void ble_start_advertising(void)
{
	static const uint8_t adv_flags = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
	static const struct bt_data ad[] = {
		BT_DATA(BT_DATA_FLAGS, &adv_flags, sizeof(adv_flags)),
	};
	static const struct bt_data sd[] = {
		BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
			(sizeof(CONFIG_BT_DEVICE_NAME) - 1U)),
	};
	int err;

	err = bt_enable(NULL);
	if (err < 0) {
		printk("BLE enable failed: %d\n", err);
		return;
	}

	err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err < 0) {
		printk("BLE adv start failed: %d\n", err);
	}
}

int main(void)
{
	const struct device *epd = DEVICE_DT_GET(EPD_NODE);
	const struct device *dbi_dev = DEVICE_DT_GET(DT_PARENT(EPD_NODE));
	struct display_capabilities caps;
	struct display_buffer_descriptor desc = {
		.width = EPD_WIDTH,
		.height = EPD_HEIGHT,
		.pitch = EPD_WIDTH,
		.buf_size = EPD_BUF_SIZE,
	};
	static uint8_t black_buf[EPD_BUF_SIZE];
	static uint8_t color_buf[EPD_BUF_SIZE];

	if (!device_is_ready(epd) || !device_is_ready(dbi_dev)) {
		printk("Display devices not ready\n");
		goto start_ble;
	}

	if (!gpio_is_ready_dt(&epd_busy)) {
		printk("EPD busy GPIO not ready\n");
		goto start_ble;
	}

	display_get_capabilities(epd, &caps);
	if ((caps.screen_info & SCREEN_INFO_MONO_VTILED) == 0U ||
	    caps.x_resolution < EPD_WIDTH || caps.y_resolution < EPD_HEIGHT) {
		printk("Display caps not supported\n");
		goto start_ble;
	}

	display_set_pixel_format(epd, PIXEL_FORMAT_MONO10);
	display_set_orientation(epd, DISPLAY_ORIENTATION_NORMAL);

	epd_fill_pattern(black_buf, color_buf, EPD_WIDTH, EPD_HEIGHT,
			 (caps.screen_info & SCREEN_INFO_MONO_MSB_FIRST) != 0U);

	display_blanking_on(epd);
	if (display_write(epd, 0, 0, &desc, black_buf) < 0) {
		printk("Display write failed\n");
		goto start_ble;
	}

	epd_busy_wait();
	if (epd_write_red_ram(dbi_dev, color_buf, EPD_WIDTH, EPD_HEIGHT) < 0) {
		printk("Red RAM write failed\n");
		goto start_ble;
	}

	if (display_blanking_off(epd) < 0) {
		printk("Display refresh failed\n");
		goto start_ble;
	}

start_ble:
	ble_start_advertising();

#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
	const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

	if (!gpio_is_ready_dt(&led)) {
		printk("LED not ready\n");
		goto done;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

	while (1) {
		gpio_pin_toggle_dt(&led);
		k_msleep(500);
	}
#endif

done:
	while (1) {
		k_msleep(1000);
	}
}

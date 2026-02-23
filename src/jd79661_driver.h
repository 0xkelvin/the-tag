#ifndef JD79661_DRIVER_H
#define JD79661_DRIVER_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>

// Display dimensions
#define EPD_WIDTH   152
#define EPD_HEIGHT  152

// 2-bit color encoding
#define COLOR_BLACK   0x00  // 00
#define COLOR_WHITE   0x01  // 01
#define COLOR_YELLOW  0x02  // 10
#define COLOR_RED     0x03  // 11

// Buffer size for 152x152 display with 2-bit per pixel
#define ALLSCREEN_BYTES   (152 * 152 / 4)  // 5776 bytes

// Driver structure
struct jd79661_device {
    const struct device *spi_dev;
    struct spi_config spi_cfg;
    struct gpio_dt_spec dc_gpio;
    struct gpio_dt_spec reset_gpio;
    struct gpio_dt_spec busy_gpio;
    struct gpio_dt_spec cs_gpio;
};

// Function prototypes
int jd79661_init(struct jd79661_device *dev);
void jd79661_sleep(struct jd79661_device *dev);
void jd79661_refresh(struct jd79661_device *dev);
void jd79661_display_all_black(struct jd79661_device *dev);
void jd79661_display_all_white(struct jd79661_device *dev);
void jd79661_display_all_yellow(struct jd79661_device *dev);
void jd79661_display_all_red(struct jd79661_device *dev);
void jd79661_display_buffer(struct jd79661_device *dev, const uint8_t *buffer);

#endif // JD79661_DRIVER_H

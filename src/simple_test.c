#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "jd79661_driver.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

// GPIO pins from overlay
#define DC_NODE    DT_NODELABEL(gpio1)
#define RESET_NODE DT_NODELABEL(gpio1)
#define BUSY_NODE  DT_NODELABEL(gpio1)
#define CS_NODE    DT_NODELABEL(gpio1)

#define DC_PIN     7   // P1.07 (D3)
#define RESET_PIN  4   // P1.04 (D0)
#define BUSY_PIN   11  // P1.11 (D5)
#define CS_PIN     5   // P1.05 (D1)

static struct jd79661_device epd_dev;

int main(void)
{
    int ret;
    
    LOG_INF("=== JD79661 Simple Test ===");
    
    // Get SPI device
    epd_dev.spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi00));
    if (!device_is_ready(epd_dev.spi_dev)) {
        LOG_ERR("SPI device not ready");
        return -1;
    }
    
    // Configure SPI (no auto CS control)
    epd_dev.spi_cfg.frequency = 4000000;  // 4 MHz
    epd_dev.spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER;
    epd_dev.spi_cfg.slave = 0;
    epd_dev.spi_cfg.cs = (struct spi_cs_control) {
        .gpio = {0},  // No auto CS
        .delay = 0,
    };
    
    // Setup GPIO pins
    epd_dev.dc_gpio.port = DEVICE_DT_GET(DC_NODE);
    epd_dev.dc_gpio.pin = DC_PIN;
    epd_dev.dc_gpio.dt_flags = GPIO_ACTIVE_HIGH;
    
    epd_dev.reset_gpio.port = DEVICE_DT_GET(RESET_NODE);
    epd_dev.reset_gpio.pin = RESET_PIN;
    epd_dev.reset_gpio.dt_flags = GPIO_ACTIVE_HIGH;
    
    epd_dev.busy_gpio.port = DEVICE_DT_GET(BUSY_NODE);
    epd_dev.busy_gpio.pin = BUSY_PIN;
    epd_dev.busy_gpio.dt_flags = GPIO_ACTIVE_HIGH;
    
    epd_dev.cs_gpio.port = DEVICE_DT_GET(CS_NODE);
    epd_dev.cs_gpio.pin = CS_PIN;
    epd_dev.cs_gpio.dt_flags = GPIO_ACTIVE_LOW;
    
    // Configure GPIO pins
    if (!device_is_ready(epd_dev.dc_gpio.port)) {
        LOG_ERR("DC GPIO not ready");
        return -1;
    }
    
    ret = gpio_pin_configure_dt(&epd_dev.dc_gpio, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure DC pin: %d", ret);
        return ret;
    }
    
    ret = gpio_pin_configure_dt(&epd_dev.reset_gpio, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure RESET pin: %d", ret);
        return ret;
    }
    
    ret = gpio_pin_configure_dt(&epd_dev.busy_gpio, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure BUSY pin: %d", ret);
        return ret;
    }
    
    ret = gpio_pin_configure_dt(&epd_dev.cs_gpio, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure CS pin: %d", ret);
        return ret;
    }
    
    LOG_INF("GPIO pins configured");
    
    // Check BUSY pin before init
    int busy = gpio_pin_get_dt(&epd_dev.busy_gpio);
    LOG_INF("BUSY pin before init: %d", busy);
    
    // Initialize display
    LOG_INF("Initializing display...");
    ret = jd79661_init(&epd_dev);
    if (ret < 0) {
        LOG_ERR("Failed to initialize display: %d", ret);
        return ret;
    }
    
    LOG_INF("Display initialized successfully");
    
    // Test: White screen only
    LOG_INF("Displaying white screen...");
    jd79661_display_all_white(&epd_dev);
    LOG_INF("White screen command sent");
    
    k_msleep(5000);  // Wait 5 seconds
    
    LOG_INF("Test complete! Check the display.");
    
    return 0;
}

#include "jd79661_driver.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(jd79661, LOG_LEVEL_INF);

// Helper functions
static void jd79661_write_cmd(struct jd79661_device *dev, uint8_t cmd)
{
    struct spi_buf tx_buf = {.buf = &cmd, .len = 1};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};
    
    gpio_pin_set_dt(&dev->cs_gpio, 1);  // CS active (low)
    gpio_pin_set_dt(&dev->dc_gpio, 0);  // Command mode
    spi_write(dev->spi_dev, &dev->spi_cfg, &tx_bufs);
    gpio_pin_set_dt(&dev->cs_gpio, 0);  // CS inactive (high)
}

static void jd79661_write_data(struct jd79661_device *dev, uint8_t data)
{
    struct spi_buf tx_buf = {.buf = &data, .len = 1};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};
    
    gpio_pin_set_dt(&dev->cs_gpio, 1);  // CS active (low)
    gpio_pin_set_dt(&dev->dc_gpio, 1);  // Data mode
    spi_write(dev->spi_dev, &dev->spi_cfg, &tx_bufs);
    gpio_pin_set_dt(&dev->cs_gpio, 0);  // CS inactive (high)
}

static void jd79661_write_data_bulk(struct jd79661_device *dev, const uint8_t *data, size_t len)
{
    gpio_pin_set_dt(&dev->cs_gpio, 1);  // CS active (low)
    gpio_pin_set_dt(&dev->dc_gpio, 1);  // Data mode
    
    // Write in chunks to avoid timeout
    const size_t chunk_size = 256;
    for (size_t i = 0; i < len; i += chunk_size) {
        size_t remaining = len - i;
        size_t write_len = (remaining < chunk_size) ? remaining : chunk_size;
        
        struct spi_buf tx_buf = {.buf = (void *)(data + i), .len = write_len};
        struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};
        spi_write(dev->spi_dev, &dev->spi_cfg, &tx_bufs);
    }
    
    gpio_pin_set_dt(&dev->cs_gpio, 0);  // CS inactive (high)
}

static void jd79661_wait_busy(struct jd79661_device *dev)
{
    // BUSY=0 means busy, BUSY=1 means idle
    int timeout = 5000;  // 5 seconds timeout
    int busy_val = gpio_pin_get_dt(&dev->busy_gpio);
    
    LOG_INF("Waiting for BUSY (initial value: %d)", busy_val);
    
    while (gpio_pin_get_dt(&dev->busy_gpio) == 0 && timeout > 0) {
        k_msleep(10);
        timeout -= 10;
    }
    
    if (timeout <= 0) {
        LOG_ERR("Timeout waiting for BUSY pin (still 0)");
    } else {
        LOG_INF("BUSY pin ready (value: 1)");
    }
}

int jd79661_init(struct jd79661_device *dev)
{
    LOG_INF("Initializing JD79661 display");
    
    // Reset sequence
    k_msleep(20);  // At least 20ms delay
    gpio_pin_set_dt(&dev->reset_gpio, 0);  // Reset low
    k_msleep(40);  // At least 40ms delay
    gpio_pin_set_dt(&dev->reset_gpio, 1);  // Reset high
    k_msleep(50);  // At least 50ms delay
    
    jd79661_wait_busy(dev);
    
    // Initialization sequence from STM32 sample code
    jd79661_write_cmd(dev, 0x4D);
    jd79661_write_data(dev, 0x78);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0x00);  // PSR
    jd79661_write_data(dev, 0x0F);
    jd79661_write_data(dev, 0x29);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0x01);  // PWRR
    jd79661_write_data(dev, 0x07);
    jd79661_write_data(dev, 0x00);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0x03);  // POFS
    jd79661_write_data(dev, 0x10);
    jd79661_write_data(dev, 0x54);
    jd79661_write_data(dev, 0x44);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0x06);  // BTST_P
    jd79661_write_data(dev, 0x05);
    jd79661_write_data(dev, 0x00);
    jd79661_write_data(dev, 0x3F);
    jd79661_write_data(dev, 0x0A);
    jd79661_write_data(dev, 0x25);
    jd79661_write_data(dev, 0x12);
    jd79661_write_data(dev, 0x1A);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0x50);  // CDI
    jd79661_write_data(dev, 0x37);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0x60);  // TCON
    jd79661_write_data(dev, 0x02);
    jd79661_write_data(dev, 0x02);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0x61);  // TRES (resolution)
    jd79661_write_data(dev, EPD_WIDTH / 256);   // Source_BITS_H
    jd79661_write_data(dev, EPD_WIDTH % 256);   // Source_BITS_L
    jd79661_write_data(dev, EPD_HEIGHT / 256);  // Gate_BITS_H
    jd79661_write_data(dev, EPD_HEIGHT % 256);  // Gate_BITS_L
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0xE7);
    jd79661_write_data(dev, 0x1C);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0xE3);
    jd79661_write_data(dev, 0x22);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0xB4);
    jd79661_write_data(dev, 0xD0);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0xB5);
    jd79661_write_data(dev, 0x03);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0xE9);
    jd79661_write_data(dev, 0x01);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0x30);
    jd79661_write_data(dev, 0x08);
    k_usleep(100);
    
    jd79661_write_cmd(dev, 0x04);  // Power on
    jd79661_wait_busy(dev);
    
    LOG_INF("JD79661 initialization complete");
    return 0;
}

void jd79661_refresh(struct jd79661_device *dev)
{
    jd79661_write_cmd(dev, 0x12);  // Display Update Control
    jd79661_write_data(dev, 0x00);
    jd79661_wait_busy(dev);
}

void jd79661_sleep(struct jd79661_device *dev)
{
    jd79661_write_cmd(dev, 0x02);  // Power off
    jd79661_wait_busy(dev);
    k_msleep(100);  // At least 100ms delay
    
    jd79661_write_cmd(dev, 0x07);  // Deep sleep
    jd79661_write_data(dev, 0xA5);
}

void jd79661_display_all_black(struct jd79661_device *dev)
{
    LOG_INF("Displaying all black");
    
    jd79661_write_cmd(dev, 0x10);  // Write data
    
    gpio_pin_set_dt(&dev->dc_gpio, 1);  // Data mode
    for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
        jd79661_write_data(dev, 0x00);  // Black = 00
    }
    
    jd79661_refresh(dev);
}

void jd79661_display_all_white(struct jd79661_device *dev)
{
    LOG_INF("Displaying all white");
    
    jd79661_write_cmd(dev, 0x10);  // Write data
    
    for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
        jd79661_write_data(dev, 0x55);  // White = 01 (0x55 = 01010101)
    }
    
    jd79661_refresh(dev);
}

void jd79661_display_all_yellow(struct jd79661_device *dev)
{
    LOG_INF("Displaying all yellow");
    
    jd79661_write_cmd(dev, 0x10);  // Write data
    
    for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
        jd79661_write_data(dev, 0xAA);  // Yellow = 10 (0xAA = 10101010)
    }
    
    jd79661_refresh(dev);
}

void jd79661_display_all_red(struct jd79661_device *dev)
{
    LOG_INF("Displaying all red");
    
    jd79661_write_cmd(dev, 0x10);  // Write data
    
    for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
        jd79661_write_data(dev, 0xFF);  // Red = 11 (0xFF = 11111111)
    }
    
    jd79661_refresh(dev);
}

void jd79661_display_buffer(struct jd79661_device *dev, const uint8_t *buffer)
{
    LOG_INF("Displaying buffer");
    
    jd79661_write_cmd(dev, 0x10);  // Write data
    jd79661_write_data_bulk(dev, buffer, ALLSCREEN_BYTES);
    jd79661_refresh(dev);
}

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "jd79661_driver.h"
#include <string.h>

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
static uint8_t framebuffer[ALLSCREEN_BYTES];

// Helper function to set pixel (x, y) with color
// Color: 0=black, 1=white, 2=yellow, 3=red
static void set_pixel(uint8_t *buffer, int x, int y, uint8_t color)
{
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) {
        return;
    }
    
    // Each byte contains 4 pixels (2 bits each)
    int byte_index = (y * EPD_WIDTH + x) / 4;
    int pixel_pos = (y * EPD_WIDTH + x) % 4;  // 0, 1, 2, or 3
    int bit_shift = (3 - pixel_pos) * 2;  // 6, 4, 2, or 0
    
    // Clear the 2 bits for this pixel
    buffer[byte_index] &= ~(0x03 << bit_shift);
    // Set the new color
    buffer[byte_index] |= (color & 0x03) << bit_shift;
}

// Draw a filled rectangle
static void draw_rect(uint8_t *buffer, int x, int y, int w, int h, uint8_t color)
{
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            set_pixel(buffer, i, j, color);
        }
    }
}

// Draw a line (simple Bresenham algorithm)
static void draw_line(uint8_t *buffer, int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        set_pixel(buffer, x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Draw a circle (Midpoint circle algorithm)
static void draw_circle(uint8_t *buffer, int cx, int cy, int radius, uint8_t color)
{
    int x = radius;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        set_pixel(buffer, cx + x, cy + y, color);
        set_pixel(buffer, cx + y, cy + x, color);
        set_pixel(buffer, cx - y, cy + x, color);
        set_pixel(buffer, cx - x, cy + y, color);
        set_pixel(buffer, cx - x, cy - y, color);
        set_pixel(buffer, cx - y, cy - x, color);
        set_pixel(buffer, cx + y, cy - x, color);
        set_pixel(buffer, cx + x, cy - y, color);
        
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

int main(void)
{
    int ret;
    
    LOG_INF("=== JD79661 Drawing Test ===");
    
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
    
    // Initialize display
    LOG_INF("Initializing display...");
    ret = jd79661_init(&epd_dev);
    if (ret < 0) {
        LOG_ERR("Failed to initialize display: %d", ret);
        return ret;
    }
    
    LOG_INF("Display initialized successfully");
    
    // Clear framebuffer to white
    memset(framebuffer, 0x55, ALLSCREEN_BYTES);  // 0x55 = white
    
    // Draw some shapes
    LOG_INF("Drawing shapes...");
    
    // Draw black border
    draw_rect(framebuffer, 0, 0, EPD_WIDTH, 2, COLOR_BLACK);  // Top
    draw_rect(framebuffer, 0, EPD_HEIGHT-2, EPD_WIDTH, 2, COLOR_BLACK);  // Bottom
    draw_rect(framebuffer, 0, 0, 2, EPD_HEIGHT, COLOR_BLACK);  // Left
    draw_rect(framebuffer, EPD_WIDTH-2, 0, 2, EPD_HEIGHT, COLOR_BLACK);  // Right
    
    // Draw red rectangle
    draw_rect(framebuffer, 20, 20, 40, 30, COLOR_RED);
    
    // Draw yellow rectangle
    draw_rect(framebuffer, 70, 20, 40, 30, COLOR_YELLOW);
    
    // Draw black circle
    draw_circle(framebuffer, 76, 90, 20, COLOR_BLACK);
    
    // Draw red circle
    draw_circle(framebuffer, 40, 90, 15, COLOR_RED);
    
    // Draw yellow circle
    draw_circle(framebuffer, 110, 90, 15, COLOR_YELLOW);
    
    // Draw diagonal lines
    draw_line(framebuffer, 10, 120, 50, 140, COLOR_BLACK);
    draw_line(framebuffer, 60, 120, 100, 140, COLOR_RED);
    draw_line(framebuffer, 110, 120, 140, 140, COLOR_YELLOW);
    
    LOG_INF("Displaying framebuffer...");
    jd79661_display_buffer(&epd_dev, framebuffer);
    
    LOG_INF("Drawing complete! Check the display.");
    
    return 0;
}

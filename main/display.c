/**
 * Display driver — ESP-IDF LCD I80 + ST7796 + LVGL v8
 * Matched to working plate-idf config: landscape 480x320
 */
#include "display.h"
#include <string.h>
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_io_i80.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "xl9555.h"

static const char *TAG = "display";

#define LCD_NUM_CS  39
#define LCD_NUM_DC  38
#define LCD_NUM_RD  14
#define LCD_NUM_WR  45
#define LCD_H_RES   480
#define LCD_V_RES   320

#define LCD_RST_IO  (1 << 0)
#define BL_CTR_IO   (1 << 4)

static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_t *display = NULL;

#define LCD_RST(x) xl9555_pin_write(LCD_RST_IO, (x) ? 1 : 0)
#define LCD_BL(x)  xl9555_pin_write(BL_CTR_IO, (x) ? 1 : 0)

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

bool display_init(void) {
    // Hardware reset via XL9555
    LCD_RST(1); vTaskDelay(pdMS_TO_TICKS(10));
    LCD_RST(0); vTaskDelay(pdMS_TO_TICKS(50));
    LCD_RST(1); vTaskDelay(pdMS_TO_TICKS(200));

    // Create I80 bus
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = LCD_NUM_DC,
        .wr_gpio_num = LCD_NUM_WR,
        .data_gpio_nums = {13,12,11,10,9,46,3,8,18,17,16,15,7,6,5,4},
        .bus_width = 16,
        .max_transfer_bytes = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
        .psram_trans_align = 64,
        .sram_trans_align = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    ESP_LOGI(TAG, "I80 bus created");

    // Create panel IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_NUM_CS,
        .pclk_hz = 25 * 1000 * 1000,
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .flags = {
            .swap_color_bytes = 0,
        },
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));
    ESP_LOGI(TAG, "Panel IO created");

    // Create ST7796 panel (uses st7789 driver, compatible)
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_LOGI(TAG, "Panel created");

    // Init panel
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_set_gap(panel_handle, 0, 0);

    // MADCTL and pixel format
    uint8_t madctl = 0x08;
    esp_lcd_panel_io_tx_param(io_handle, 0x36, &madctl, 1);
    uint8_t pixfmt = 0x55;
    esp_lcd_panel_io_tx_param(io_handle, 0x3A, &pixfmt, 1);

    // Landscape: swap_xy=true, mirror=false
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, false, false);

    esp_lcd_panel_disp_on_off(panel_handle, true);
    ESP_LOGI(TAG, "LCD: %dx%d landscape", LCD_H_RES, LCD_V_RES);

    // Init LVGL v8
    lv_init();

    // DMA-aligned buffer in internal RAM
    size_t buf_size = LCD_H_RES * 10 * sizeof(lv_color_t);
    void *buf1 = heap_caps_aligned_alloc(64, buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf1);
    memset(buf1, 0, buf_size);

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, LCD_H_RES * 10);

    // Display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    display = lv_disp_drv_register(&disp_drv);

    LCD_BL(1);
    ESP_LOGI(TAG, "LVGL v8 ready");
    return true;
}

lv_disp_t *display_get(void) {
    return display;
}

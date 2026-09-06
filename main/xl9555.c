#include "xl9555.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "xl9555";
static i2c_master_dev_handle_t dev_handle = NULL;
static uint8_t out0 = 0xFF, out1 = 0xFF;

static bool write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev_handle, buf, 2, 100) == ESP_OK;
}

void xl9555_pin_write(uint8_t pin_mask, uint8_t value) {
    if (pin_mask & 0xF0) {
        if (value) out1 |= (pin_mask >> 4); else out1 &= ~(pin_mask >> 4);
        write_reg(0x03, out1);
    } else {
        if (value) out0 |= pin_mask; else out0 &= ~pin_mask;
        write_reg(0x02, out0);
    }
}

bool xl9555_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = XL9555_SDA,
        .scl_io_num = XL9555_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &(i2c_device_config_t){
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = XL9555_ADDR,
        .scl_speed_hz = 400000,
    }, &dev_handle));

    write_reg(0x06, 0x00); write_reg(0x07, 0x00);  // all output
    write_reg(0x02, 0xFF); write_reg(0x03, 0xFF);  // all high
    ESP_LOGI(TAG, "XL9555 init done");
    return true;
}

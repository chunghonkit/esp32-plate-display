#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"

#define XL9555_ADDR  0x20
#define XL9555_SDA   2
#define XL9555_SCL   1

bool xl9555_init(void);
void xl9555_pin_write(uint8_t pin_mask, uint8_t value);

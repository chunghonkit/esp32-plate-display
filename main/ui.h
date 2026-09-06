#pragma once
#include "lvgl.h"

void ui_init(void);
void ui_set_plate(const char *plate);
void ui_set_message(const char *text);
void ui_set_status(bool connected);
void ui_set_info(const char *text);

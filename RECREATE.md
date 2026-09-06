# ESP32-S3 License Plate Display — Recreation Guide

**Goal**: Build a license plate display for car park entry monitoring on an ESP32-S3. The device subscribes to an MQTT topic and shows received plate numbers on a 3.5" ST7796 TFT via LVGL.

## Hardware
- Board: Waiken-Smart ESP32-S3-N16R8 (16 MB flash, 8 MB PSRAM)
- Display: 3.5" TFT, ST7796 controller, 320×480, 16-bit parallel, driven landscape 480×320
- GPIO expander: XL9555 over I2C (controls LCD reset + backlight)
- LVGL 8.4.0 (v8 API only)

## Pin mapping (ESP-IDF)

| Function | GPIO | Function | GPIO |
|----------|------|----------|------|
| LCD CS | 39 | LCD DC | 38 |
| LCD WR | 45 | LCD RD | 14 |
| LCD D0–D15 | 13,12,11,10,9,46,3,8,18,17,16,15,7,6,5,4 | | |
| XL9555 SCL | 1 | XL9555 SDA | 2 |

## Behaviour
1. Boot → LCD initialises immediately and shows the UI (blue header "BVS3 CARPARK ENTRY", big cyan plate text) in **both** AP and STA paths.
2. WiFi priority: NVS-saved credentials → hardcoded defaults → fallback AP "PlateDisplay" (captive portal at http://192.168.4.1, HTTP GET form, saves to NVS with read-back verify, then reboots).
3. Once on WiFi → connect MQTT → subscribe to topic → render incoming plate text — holds ~20 s then clears to `---`; a new plate refreshes the window. Status bar green = connected / red = disconnected.

## Software stack
- ESP-IDF v5.4 (target `esp32s3`), CMake + Ninja
- LVGL 8.4.0 as a component (git submodule under `components/lvgl`)
- Panel driver: use `esp_lcd_new_panel_ili9486()` init for the ST7796 — `st7789` init produces garbled output
- I80 (Intel 8080) 16-bit parallel LCD bus; landscape = 480×320 with `swap_xy = true` (wrong dims garble the right third)

## MQTT
All MQTT settings are `#define`s at the top of `main/mqtt_handler.c`:
```c
#define MQTT_BROKER    "mqtt://<your-broker-host>:1883"
#define MQTT_USER      "<your-user>"
#define MQTT_PASS_STR  "<your-password>"
#define MQTT_TOPIC     "bvs3/carpark/entry"
```
Topic and message format are plain text (e.g. `GBA009`). Replace host/user/pass with deployment values — do not hardcode production credentials in a public repo.

## Source layout
- `main/main.c` — entry, WiFi STA/AP + captive portal
- `main/mqtt_handler.c` — MQTT client + WiFi STA connect
- `main/display.c` — ST7796 I80 LCD init + LVGL
- `main/ui.c` — LVGL UI (header, plate label, status bar)
- `main/xl9555.c` — GPIO expander driver (reset/backlight)
- `components/lvgl/` — LVGL 8.4.0 submodule
- `partitions.csv` — 16 MB flash layout

## Build & flash
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```
(or `cmake -B build -G Ninja -DIDF_TARGET=esp32s3 && cmake --build build`, flash via `esptool.py --chip esp32s3 write_flash @flash_args`).

## Key configuration (sdkconfig)
- 16 MB flash (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB`), octal PSRAM 8 MB
- I80 LCD bus width 16 (CONFIG_LCD_I80_BUS_WIDTH / CONFIG_LCDCAM_I80_BUS_WIDTH = 16)
- LVGL component enabled; v8 API

## Acceptance test
Publish a plate string (e.g. `GBA009`) to the MQTT topic → it appears large on the display; disconnecting the broker turns the status bar red.

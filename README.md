# ESP32-S3 License Plate Display

A license plate display system for car park entry monitoring. The ESP32-S3 subscribes to an MQTT topic and displays detected plate numbers on a 3.5" ST7796 TFT LCD via LVGL.

## Features

- **WiFi Captive Portal** — First-time setup via web browser
- **MQTT Subscription** — Real-time plate number display
- **LVGL Display** — Smooth 480×320 landscape UI with large fonts
- **NVS Storage** — WiFi credentials persist across reboots
- **Fallback WiFi** — Default credentials → saved credentials → AP portal

## Hardware

- **Board:** Waiken-Smart ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM)
- **Display:** 3.5" TFT LCD (320×480, ST7796 controller, 16-bit parallel)
- **GPIO Expander:** XL9555 (I2C, controls LCD reset + backlight)

### Pin Mapping

| Function | Pin | Function | Pin |
|----------|-----|----------|-----|
| LCD CS | GPIO39 | LCD DC | GPIO38 |
| LCD WR | GPIO45 | LCD RD | GPIO14 |
| D0-D15 | GPIO13,12,11,10,9,46,3,8,18,17,16,15,7,6,5,4 | | |
| XL9555 SCL | GPIO1 | XL9555 SDA | GPIO2 |

## WiFi Configuration

### How It Works

1. **Boot** — Display initializes immediately (shows UI)
2. **WiFi Priority:**
   - Try NVS saved credentials (from captive portal)
   - If none → try default hardcoded credentials
   - If both fail → start AP "PlateDisplay"
3. **Captive Portal** — Connect to "PlateDisplay" WiFi, open http://192.168.4.1
4. **After Setup** — Device reboots, connects to saved WiFi, starts MQTT

### Changing WiFi

- **New location:** Default WiFi fails → AP starts automatically → enter new credentials
- **Reset:** Erase flash (`esptool.py erase_flash`) → AP starts on next boot

## MQTT Configuration

### Topic
```
bvs3/carpark/entry
```

### Message Format
Send plate number as plain text:
```
GBA009
```

### Broker
- Port: 1883
- User: `mqtt`

## Display Layout

```
┌────────────────────────────────────┐
│         BVS3 CARPARK ENTRY         │  ← Header (Montserrat 18)
├────────────────────────────────────┤
│                                    │
│           GBA009                   │  ← Plate Number (Montserrat 48)
│                                    │
│              ●                 │  ← Status LED (green/grey)
├────────────────────────────────────┤
└────────────────────────────────────┘
```

- **Header:** Blue background, white "BVS3 CARPARK ENTRY" text
- **Plate Number:** Large cyan text, holds for 20 seconds
- **Status LED:** Green dot = MQTT connected, light grey dot = disconnected

## Building

### Prerequisites

- ESP-IDF v5.4
- Python 3.11+

### Build & Flash

```bash
# Set up environment
export IDF_PATH=/root/esp-idf
export IDF_TOOLS_PATH=/root/.espressif
export PATH="/root/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin:$PATH"

# Clone and build
git clone https://github.com/chunghonkit/esp32-plate-display.git
cd esp32-plate-display
git submodule update --init
cmake -B build -G Ninja -DIDF_TARGET=esp32s3
cmake --build build

# Flash
cd build
esptool.py --chip esp32s3 -b 460800 write_flash @flash_args
```

### Project Structure

```
├── main/
│   ├── main.c            # Entry point + WiFi logic
│   ├── display.c         # LCD driver (ST7796 + LVGL v8)
│   ├── mqtt_handler.c    # WiFi STA + MQTT client
│   ├── ui.c              # LVGL UI components
│   └── xl9555.c          # GPIO expander driver
├── components/
│   └── lvgl/             # LVGL 8.4.0 (git submodule)
├── sdkconfig             # Working configuration
└── partitions.csv        # 16MB flash partition table
```

## Customization

### Change Default WiFi

Edit `main/main.c`:
```c
#define DEFAULT_WIFI_SSID  "YourSSID"
#define DEFAULT_WIFI_PASS  "YourPassword"
```

### Change MQTT Broker

Edit `main/mqtt_handler.c`:
```c
#define MQTT_BROKER_URI "mqtt://your-broker:1883"
#define MQTT_USERNAME    "your_user"
#define MQTT_PASSWORD    "your_pass"
#define MQTT_TOPIC       "your/topic"
```

### Change AP Name/Password

Edit `main/main.c`:
```c
#define AP_SSID  "YourAPName"
#define AP_PASS  "YourPassword"
```

## License

MIT

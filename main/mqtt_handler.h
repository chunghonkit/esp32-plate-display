#pragma once

typedef void (*plate_received_cb)(const char *plate);

// Start MQTT (WiFi handled externally)
void mqtt_handler_init(const char *ssid, const char *pass, plate_received_cb cb);

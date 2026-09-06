/**
 * MQTT handler — WiFi already connected by caller
 */
#include "mqtt_handler.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt";
#define MQTT_BROKER    "mqtt://mqtt.citybaseiot.duckdns.org:1883"
#define MQTT_USER      "mqtt"
#define MQTT_PASS_STR  "bvs21910323"
#define MQTT_TOPIC     "bvs3/carpark/entry"

static esp_mqtt_client_handle_t mqtt_client = NULL;
static plate_received_cb on_plate = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC, 1);
            break;
        case MQTT_EVENT_DATA:
            if (event->topic_len > 0 && strncmp(event->topic, MQTT_TOPIC, event->topic_len) == 0) {
                char msg[64] = {0};
                int len = event->data_len < 63 ? event->data_len : 63;
                memcpy(msg, event->data, len);
                msg[len] = '\0';
                ESP_LOGI(TAG, "Plate: %s", msg);
                if (on_plate) on_plate(msg);
            }
            break;
        default: break;
    }
}

void mqtt_handler_init(const char *ssid, const char *pass, plate_received_cb cb) {
    on_plate = cb;
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
        .credentials.username = MQTT_USER,
        .credentials.authentication.password = MQTT_PASS_STR,
        .network.reconnect_timeout_ms = 5000,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "MQTT client started");
}

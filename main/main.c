/**
 * ESP32-S3 License Plate Display — WiFi AP + HTTP portal + NVS save
 * Built on vendor's working WiFi AP pattern
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include <netdb.h>

#include "xl9555.h"
#include "display.h"
#include "mqtt_handler.h"
#include "ui.h"

static const char *TAG = "main";

// === Default WiFi (fallback if no saved creds) ===
#define DEFAULT_WIFI_SSID  "msc_24G"
#define DEFAULT_WIFI_PASS  "12345678"

// === Captive Portal AP ===
#define AP_SSID  "PlateDisplay"
#define AP_PASS  "12345678"

// === NVS ===
#define NVS_NAMESPACE  "wifi_creds"
#define NVS_KEY_SSID   "ssid"
#define NVS_KEY_PASS   "password"

static esp_err_t wifi_creds_load(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) { ESP_LOGW(TAG, "NVS open failed: %d", err); return err; }
    err = nvs_get_str(h, NVS_KEY_SSID, ssid, &ssid_len);
    if (err != ESP_OK) { ESP_LOGW(TAG, "NVS read ssid failed: %d", err); nvs_close(h); return err; }
    err = nvs_get_str(h, NVS_KEY_PASS, pass, &pass_len);
    if (err != ESP_OK) { ESP_LOGW(TAG, "NVS read pass failed: %d", err); nvs_close(h); return err; }
    nvs_close(h);
    ESP_LOGI(TAG, "NVS loaded ssid='%s'", ssid);
    return ESP_OK;
}

static esp_err_t wifi_creds_save(const char *ssid, const char *pass) {
    ESP_LOGI(TAG, "Saving WiFi: ssid='%s'", ssid);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) { ESP_LOGE(TAG, "NVS open RW failed: %d", err); return err; }
    err = nvs_set_str(h, NVS_KEY_SSID, ssid);
    if (err != ESP_OK) { ESP_LOGE(TAG, "NVS set ssid failed: %d", err); nvs_close(h); return err; }
    err = nvs_set_str(h, NVS_KEY_PASS, pass);
    if (err != ESP_OK) { ESP_LOGE(TAG, "NVS set pass failed: %d", err); nvs_close(h); return err; }
    err = nvs_commit(h);
    if (err != ESP_OK) { ESP_LOGE(TAG, "NVS commit failed: %d", err); nvs_close(h); return err; }
    nvs_close(h);
    ESP_LOGI(TAG, "NVS saved OK: ssid='%s'", ssid);

    // Verify read-back
    nvs_handle_t h2;
    char verify[33] = {0};
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h2) == ESP_OK) {
        size_t sz = sizeof(verify);
        if (nvs_get_str(h2, NVS_KEY_SSID, verify, &sz) == ESP_OK) {
            ESP_LOGI(TAG, "NVS verify: ssid='%s'", verify);
        } else {
            ESP_LOGE(TAG, "NVS verify FAILED!");
        }
        nvs_close(h2);
    }
    return ESP_OK;
}

// === HTTP ===
static const char page_html[] = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head>"
    "<body style='font-family:sans-serif;text-align:center;margin-top:40px'>"
    "<h1>Plate Display</h1>"
    "<h3>WiFi Setup</h3>"
    "<form method='GET' action='/save'>"
    "<input name='ssid' placeholder='WiFi SSID' style='width:200px;padding:8px;margin:5px'><br>"
    "<input name='pass' type='password' placeholder='Password' style='width:200px;padding:8px;margin:5px'><br><br>"
    "<button type='submit' style='padding:10px 30px;font-size:16px'>Save &amp; Reboot</button></form></body></html>";
static const char ok_html[] = "<html><body style='font-family:sans-serif;text-align:center;margin-top:80px'>"
    "<h2>WiFi Saved!</h2><p>Rebooting now...</p></body></html>";

static esp_err_t h_index(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, page_html, strlen(page_html));
}

static esp_err_t h_save(httpd_req_t *req) {
    char query[256] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));
    ESP_LOGI(TAG, "Raw query: '%s'", query);

    char ssid[33] = {0}, pass[65] = {0};
    httpd_query_key_value(query, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(query, "pass", pass, sizeof(pass));
    ESP_LOGI(TAG, "Parsed: ssid='%s' pass='%s'", ssid, pass);

    if (strlen(ssid) == 0) {
        ESP_LOGE(TAG, "Empty SSID, not saving");
        httpd_resp_set_type(req, "text/html");
        return httpd_resp_send(req, "<html><body><h1>Error: empty SSID</h1><a href='/'>Back</a></body></html>", -1);
    }

    wifi_creds_save(ssid, pass);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, ok_html, strlen(ok_html));

    ESP_LOGI(TAG, "Rebooting in 2s...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

// === WiFi AP ===
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)data;
        ESP_LOGI(TAG, "station joined, AID=%d", e->aid);
    }
}

static void start_ap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = { .ap = {
        .ssid = AP_SSID, .ssid_len = strlen(AP_SSID),
        .password = AP_PASS, .max_connection = 5,
        .authmode = WIFI_AUTH_WPA_WPA2_PSK,
    }};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "AP started, IP: %s", ip_addr);

    // Start HTTP server
    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    http_cfg.max_uri_handlers = 8;
    http_cfg.stack_size = 8192;
    httpd_handle_t server = NULL;
    httpd_start(&server, &http_cfg);
    httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/", .method=HTTP_GET, .handler=h_index});
    httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/save", .method=HTTP_GET, .handler=h_save});
    httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/*", .method=HTTP_GET, .handler=h_index});
    ESP_LOGI(TAG, "HTTP server ready at http://%s", ip_addr);
}

// === WiFi STA ===
static volatile bool s_got_ip = false;
static void sta_event_cb(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "STA starting...");
        esp_wifi_connect();
    } else if (id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        s_got_ip = true;
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA disconnected, reconnecting...");
        esp_wifi_connect();
    }
}

static void start_sta(const char *ssid, const char *pass) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, sta_event_cb, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, sta_event_cb, NULL));

    wifi_config_t sta_cfg = { 0 };
    strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password) - 1);
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to '%s'...", ssid);
}

// === Plate display callback ===
static void on_plate(void *data) {
    char *plate = (char *)data;
    ui_set_plate(plate);
    free(plate);
}
static void plate_cb(const char *plate) {
    char *copy = strdup(plate);
    if (copy) lv_async_call(on_plate, copy);
}

// === Entry point ===
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // Init display always (shows something on screen either way)
    xl9555_init();
    display_init();
    ui_init();

    // WiFi priority: NVS saved → default hardcoded → AP captive portal
    char wifi_ssid[33] = {0};
    char wifi_pass[65] = {0};

    // 1. Try NVS saved credentials
    ret = wifi_creds_load(wifi_ssid, sizeof(wifi_ssid), wifi_pass, sizeof(wifi_pass));

    // 2. If no saved creds, try default hardcoded
    if (ret != ESP_OK || strlen(wifi_ssid) == 0) {
        ESP_LOGI(TAG, "No saved WiFi, using default: %s", DEFAULT_WIFI_SSID);
        strncpy(wifi_ssid, DEFAULT_WIFI_SSID, sizeof(wifi_ssid) - 1);
        strncpy(wifi_pass, DEFAULT_WIFI_PASS, sizeof(wifi_pass) - 1);
    }

    // 3. Try connecting with whatever credentials we have
    ESP_LOGI(TAG, "WiFi: %s — connecting", wifi_ssid);
    ui_set_status(false);
    esp_task_wdt_delete(NULL);

    start_sta(wifi_ssid, wifi_pass);

    // Wait for WiFi connection (up to 15s)
    for (int i = 0; i < 150 && !s_got_ip; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (s_got_ip) {
        // === WiFi connected — plate display mode ===
        ESP_LOGI(TAG, "WiFi connected, starting MQTT");
        ui_set_status(true);
        mqtt_handler_init(wifi_ssid, wifi_pass, plate_cb);
    } else {
        // === WiFi failed — start AP captive portal ===
        ESP_LOGW(TAG, "WiFi connect failed — starting captive portal");
        ui_set_plate("Setup WiFi");
        ui_set_info("Connect to: PlateDisplay");
        start_ap();
    }

    // LVGL loop (runs forever in either mode)
    ESP_LOGI(TAG, "Entering LVGL loop");
    while (1) {
        lv_tick_inc(10);
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_timer_handler();
    }
}

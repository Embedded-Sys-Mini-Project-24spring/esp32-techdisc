#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include <esp_http_server.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_wifi_types.h"


#define EXAMPLE_ESP_WIFI_SSID      "ESP32-S2" //CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      "mypassword" //CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_MAX_STA_CONN        5 //CONFIG_ESP_MAX_STA_CONN

static void generate_async_hello_resp(void *arg);

static esp_err_t async_get_handler(httpd_req_t *req);

static void ws_async_send(void *arg);

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req);

static esp_err_t echo_handler(httpd_req_t *req);

static const httpd_uri_t ws;

static httpd_handle_t start_webserver(void);

static esp_err_t stop_webserver(httpd_handle_t server);

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);


static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data);

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

esp_err_t wifi_init_softap(void);

esp_err_t queue_send(char* data_str);

static void send_data_to_all();

void wifi_server_init(void (*cmd_hd)(unsigned char*));

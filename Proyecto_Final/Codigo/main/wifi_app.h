/*
 * wifi_app.h
 *
 * Gestión WiFi para STR 2026 — Modo dual AP+STA.
 */

#ifndef WIFI_APP_H
#define WIFI_APP_H

#include <stdbool.h>
#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"

// =====================================================
// Parámetros del Soft-AP
// =====================================================
#define WIFI_AP_SSID                "STR_2026"
#define WIFI_AP_PASSWORD            "str2026pass"
#define WIFI_AP_CHANNEL             1
#define WIFI_AP_SSID_HIDDEN         0
#define WIFI_AP_MAX_CONNECTIONS     4
#define WIFI_AP_BEACON_INTERVAL     100
#define WIFI_AP_IP                  "192.168.4.1"
#define WIFI_AP_GATEWAY             "192.168.4.1"
#define WIFI_AP_NETMASK             "255.255.255.0"
#define WIFI_AP_BANDWIDTH           WIFI_BW_HT20
#define WIFI_STA_POWER_SAVE         WIFI_PS_NONE

// =====================================================
// Límites estándar IEEE
// =====================================================
#define MAX_SSID_LENGTH             32
#define MAX_PASSWORD_LENGTH         64
#define MAX_CONNECTION_RETRIES      5

// =====================================================
// Objetos netif (accesibles desde http_server.c)
// =====================================================
extern esp_netif_t *esp_netif_sta;
extern esp_netif_t *esp_netif_ap;

// =====================================================
// IDs de mensajes para la cola de la tarea WiFi
// =====================================================
typedef enum {
    WIFI_APP_MSG_START_HTTP_SERVER = 0,
    WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER,
    WIFI_APP_MSG_STA_CONNECTED_GOT_IP,
    WIFI_APP_MSG_STA_DISCONNECTED,
    WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT,
} wifi_app_message_e;

typedef struct {
    wifi_app_message_e msgID;
} wifi_app_queue_message_t;

// =====================================================
// API pública
// =====================================================

void wifi_app_start(void);
BaseType_t wifi_app_send_message(wifi_app_message_e msgID);
wifi_config_t *wifi_app_get_wifi_config(void);
esp_err_t wifi_app_load_sta_credentials(char *ssid, char *password);
void wifi_app_save_sta_credentials(const char *ssid, const char *password);

// NTP — sincronización horaria
void init_obtain_time(void);
bool get_state_time_was_synchronized(void);

#endif /* WIFI_APP_H */

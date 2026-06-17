/*
 * wifi_app.c
 *
 * Gestion WiFi para STR 2026 — ESP32-C6 / ESP-IDF v5.4
 * Modo dual: Soft-AP siempre activo + STA opcional.
 *
 * Flujo:
 *   1. wifi_app_start() lanza wifi_app_task.
 *   2. La tarea inicializa el driver, levanta el AP y arranca el HTTP server.
 *   3. Cuando el usuario envia credenciales STA desde la web, llega el mensaje
 *      WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER y se intenta la conexion.
 *   4. Si el ESP32 ya tenia credenciales guardadas en NVS, las intenta al arranque.
 */

#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/sockets.h"
#include "arpa/inet.h"

#include "http_server.h"
#include "rgb.h"
#include "rgb_led.h"
#include "tasks_common.h"
#include "wifi_app.h"

static const char *TAG = "wifi_app";

// Clave NVS para credenciales STA
#define NVS_NAMESPACE   "wifi_cfg"
#define NVS_KEY_SSID    "sta_ssid"
#define NVS_KEY_PASS    "sta_pass"

// Objetos netif globales
esp_netif_t *esp_netif_sta = NULL;
esp_netif_t *esp_netif_ap  = NULL;

// Cola de mensajes y configuracion WiFi
static QueueHandle_t  wifi_app_queue_handle;
static wifi_config_t *wifi_config = NULL;

// Contador de reintentos de conexion STA
static int g_retry_number = 0;

// Flag para evitar reintentos cuando la desconexion es intencional
static bool g_user_disconnect = false;

// NTP — sincronizacion horaria
static bool time_was_synchronized = false;

// =====================================================
// NTP — obtener hora de Internet
// =====================================================

void init_obtain_time(void)
{
    time_was_synchronized = false;
}

bool get_state_time_was_synchronized(void)
{
    return time_was_synchronized;
}

static void obtain_time(void)
{
    setenv("TZ", "COT5", 1);
    tzset();

    ESP_LOGI(TAG, "Inicializando SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    time_t    now     = 0;
    struct tm timeinfo = {0};
    int       retry   = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Esperando hora del sistema... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry < retry_count)
    {
        ESP_LOGI(TAG, "Hora sincronizada");
        time_was_synchronized = true;
    }
    else
    {
        ESP_LOGE(TAG, "Fallo sincronizacion SNTP");
    }
}

// =====================================================
// NVS — guardar y cargar credenciales STA
// =====================================================

void wifi_app_save_sta_credentials(const char *ssid, const char *password)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, NVS_KEY_SSID, ssid);
    nvs_set_str(h, NVS_KEY_PASS, password);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Credenciales STA guardadas: %s", ssid);
}

esp_err_t wifi_app_load_sta_credentials(char *ssid, char *password)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (ret != ESP_OK) return ESP_ERR_NVS_NOT_FOUND;

    size_t ssid_len = MAX_SSID_LENGTH;
    size_t pass_len = MAX_PASSWORD_LENGTH;

    ret  = nvs_get_str(h, NVS_KEY_SSID, ssid, &ssid_len);
    ret |= nvs_get_str(h, NVS_KEY_PASS, password, &pass_len);
    nvs_close(h);

    if (ret != ESP_OK) return ESP_ERR_NVS_NOT_FOUND;
    ESP_LOGI(TAG, "Credenciales STA cargadas: %s", ssid);
    return ESP_OK;
}

// =====================================================
// Handler de eventos WiFi y IP
// =====================================================

static void wifi_app_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "AP iniciado — SSID: %s", WIFI_AP_SSID);
            break;

        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Cliente conectado al AP — AID=%d", e->aid);
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Cliente desconectado del AP — AID=%d", e->aid);
            break;
        }

        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA iniciado");
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "STA conectado al router");
            g_retry_number = 0;
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t *e = (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGW(TAG, "STA desconectado — razon: %d", e->reason);

            // Si fue desconexion solicitada por el usuario, no reintentar
            if (g_user_disconnect)
            {
                break;
            }

            if (g_retry_number < MAX_CONNECTION_RETRIES)
            {
                g_retry_number++;
                ESP_LOGI(TAG, "Reintento %d/%d", g_retry_number, MAX_CONNECTION_RETRIES);
                esp_wifi_connect();
            }
            else
            {
                ESP_LOGE(TAG, "Maximo de reintentos alcanzado");
                wifi_app_send_message(WIFI_APP_MSG_STA_DISCONNECTED);
            }
            break;
        }

        default:
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&e->ip_info.ip));
        g_retry_number = 0;
        wifi_app_send_message(WIFI_APP_MSG_STA_CONNECTED_GOT_IP);
    }
}

// =====================================================
// Inicializacion del driver WiFi
// =====================================================

static void wifi_app_init(void)
{
    // Inicializar la pila TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Crear interfaces netif
    esp_netif_ap  = esp_netif_create_default_wifi_ap();
    esp_netif_sta = esp_netif_create_default_wifi_sta();

    // Configurar IP estatica del AP
    esp_netif_ip_info_t ap_ip_info;
    memset(&ap_ip_info, 0, sizeof(ap_ip_info));
    esp_netif_dhcps_stop(esp_netif_ap);
    inet_pton(AF_INET, WIFI_AP_IP,      &ap_ip_info.ip);
    inet_pton(AF_INET, WIFI_AP_GATEWAY, &ap_ip_info.gw);
    inet_pton(AF_INET, WIFI_AP_NETMASK, &ap_ip_info.netmask);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));

    // Registrar handlers de eventos
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_app_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_app_event_handler, NULL, NULL));

    // Inicializar driver WiFi con config por defecto
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_AP_BANDWIDTH));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_STA_POWER_SAVE));

    // Configurar el AP
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid            = WIFI_AP_SSID,
            .password        = WIFI_AP_PASSWORD,
            .ssid_len        = strlen(WIFI_AP_SSID),
            .channel         = WIFI_AP_CHANNEL,
            .ssid_hidden     = WIFI_AP_SSID_HIDDEN,
            .max_connection  = WIFI_AP_MAX_CONNECTIONS,
            .beacon_interval = WIFI_AP_BEACON_INTERVAL,
            .authmode        = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi inicializado. AP activo: SSID=%s  IP=%s", WIFI_AP_SSID, WIFI_AP_IP);
}

// =====================================================
// Intento de conexion STA
// =====================================================

static void wifi_app_connect_sta(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

// =====================================================
// Tarea principal WiFi
// =====================================================

static void wifi_app_task(void *pvParameters)
{
    wifi_app_queue_message_t msg;

    // Inicializar driver y levantar AP
    wifi_app_init();

    // Indicar estado en LED RGB de WiFi
    rgb_led_wifi_app_started();

    // Intentar conexion automatica con credenciales guardadas
    char saved_ssid[MAX_SSID_LENGTH]     = {0};
    char saved_pass[MAX_PASSWORD_LENGTH] = {0};

    if (wifi_app_load_sta_credentials(saved_ssid, saved_pass) == ESP_OK)
    {
        memcpy(wifi_config->sta.ssid,     saved_ssid, strlen(saved_ssid));
        memcpy(wifi_config->sta.password, saved_pass, strlen(saved_pass));
        ESP_LOGI(TAG, "Conectando automaticamente a: %s", saved_ssid);
        wifi_app_connect_sta();
    }

    // Arrancar el servidor HTTP
    wifi_app_send_message(WIFI_APP_MSG_START_HTTP_SERVER);

    // Bucle principal de la tarea
    for (;;)
    {
        if (xQueueReceive(wifi_app_queue_handle, &msg, portMAX_DELAY) == pdTRUE)
        {
            switch (msg.msgID)
            {
            case WIFI_APP_MSG_START_HTTP_SERVER:
                ESP_LOGI(TAG, "Iniciando servidor HTTP");
                http_server_start();
                break;

            case WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER:
            {
                ESP_LOGI(TAG, "Intento de conexion STA desde HTTP");

                char ssid[MAX_SSID_LENGTH]     = {0};
                char pass[MAX_PASSWORD_LENGTH] = {0};

                if (wifi_app_load_sta_credentials(ssid, pass) == ESP_OK)
                {
                    memset(wifi_config, 0, sizeof(wifi_config_t));
                    memcpy(wifi_config->sta.ssid,     ssid, strlen(ssid));
                    memcpy(wifi_config->sta.password, pass, strlen(pass));
                    ESP_LOGI(TAG, "Conectando a: %s", ssid);
                }

                g_retry_number = 0;
                g_user_disconnect = false;
                wifi_app_connect_sta();
                http_server_monitor_send_message(HTTP_MSG_WIFI_CONNECT_INIT);
                break;
            }

            case WIFI_APP_MSG_STA_CONNECTED_GOT_IP:
                ESP_LOGI(TAG, "STA conectado con IP");
                rgb_led_wifi_connected();
                obtain_time();
                http_server_monitor_send_message(HTTP_MSG_WIFI_CONNECT_SUCCESS);
                break;

            case WIFI_APP_MSG_STA_DISCONNECTED:
                ESP_LOGW(TAG, "STA desconectado definitivamente");
                http_server_monitor_send_message(HTTP_MSG_WIFI_CONNECT_FAIL);
                break;

            case WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT:
                ESP_LOGI(TAG, "Desconexion solicitada por el usuario");
                g_user_disconnect = true;
                esp_wifi_disconnect();
                break;

            default:
                break;
            }
        }
    }
}

// =====================================================
// API publica
// =====================================================

BaseType_t wifi_app_send_message(wifi_app_message_e msgID)
{
    wifi_app_queue_message_t msg = { .msgID = msgID };
    return xQueueSend(wifi_app_queue_handle, &msg, portMAX_DELAY);
}

wifi_config_t *wifi_app_get_wifi_config(void)
{
    return wifi_config;
}

void wifi_app_start(void)
{
    ESP_LOGI(TAG, "Iniciando modulo WiFi");

    // Suprimir logs internos de WiFi en consola (muy verbosos)
    esp_log_level_set("wifi", ESP_LOG_WARN);

    // Reservar y limpiar la estructura de configuracion WiFi
    wifi_config = (wifi_config_t *)malloc(sizeof(wifi_config_t));
    configASSERT(wifi_config != NULL);
    memset(wifi_config, 0, sizeof(wifi_config_t));

    // Crear cola de mensajes
    wifi_app_queue_handle = xQueueCreate(3, sizeof(wifi_app_queue_message_t));
    configASSERT(wifi_app_queue_handle != NULL);

    // Lanzar tarea WiFi
    xTaskCreatePinnedToCore(
        wifi_app_task,
        "wifi_app_task",
        WIFI_APP_TASK_STACK_SIZE,
        NULL,
        WIFI_APP_TASK_PRIORITY,
        NULL,
        WIFI_APP_TASK_CORE_ID
    );
}

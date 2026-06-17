/*
 * http_server.c
 *
 *  Created on: Oct 20, 2021
 *      Author: kjagu
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "sys/param.h"
#include "driver/gpio.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "http_server.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "rgb_led.h"
#include "rgb.h"
#include "esp_wifi.h"
#include "ntc.h"
#include "cJSON.h"

// Tag used for ESP serial console messages
static const char TAG[] = "http_server";

// Firmware update status
static int g_fw_update_status = OTA_UPDATE_PENDING;

// HTTP server task handle
static httpd_handle_t http_server_handle = NULL;

// HTTP server monitor task handle
static TaskHandle_t task_http_server_monitor = NULL;

// Queue handle used to manipulate the main queue of events
static QueueHandle_t http_server_monitor_queue_handle;

static uint8_t s_led_state = 0;

// Pointer to the shared system configuration
static system_config_t *g_cfg = NULL;

/**
 * ESP32 timer configuration passed to esp_timer_create.
 */
const esp_timer_create_args_t fw_update_reset_args = {
		.callback = &http_server_fw_update_reset_callback,
		.arg = NULL,
		.dispatch_method = ESP_TIMER_TASK,
		.name = "fw_update_reset"
};
esp_timer_handle_t fw_update_reset;

// Embedded files: JQuery, index.html, app.css, app.js and favicon.ico files
extern const uint8_t jquery_3_3_1_min_js_start[]	asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[]		asm("_binary_jquery_3_3_1_min_js_end");
extern const uint8_t index_html_start[]				asm("_binary_index_html_start");
extern const uint8_t index_html_end[]				asm("_binary_index_html_end");
extern const uint8_t app_css_start[]				asm("_binary_app_css_start");
extern const uint8_t app_css_end[]					asm("_binary_app_css_end");
extern const uint8_t app_js_start[]					asm("_binary_app_js_start");
extern const uint8_t app_js_end[]					asm("_binary_app_js_end");
extern const uint8_t favicon_ico_start[]			asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]				asm("_binary_favicon_ico_end");

/**
 * Checks the g_fw_update_status and creates the fw_update_reset timer if g_fw_update_status is true.
 */
static void http_server_fw_update_reset_timer(void)
{
	if (g_fw_update_status == OTA_UPDATE_SUCCESSFUL)
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW updated successful starting FW update reset timer");

		// Give the web page a chance to receive an acknowledge back and initialize the timer
		ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
		ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000));
	}
	else
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update unsuccessful");
	}
}

/**
 * HTTP server monitor task used to track events of the HTTP server
 * @param pvParameters parameter which can be passed to the task.
 */
static void http_server_monitor(void *parameter)
{
	http_server_queue_message_t msg;

	for (;;)
	{
		if (xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY))
		{
			switch (msg.msgID)
			{
				case HTTP_MSG_WIFI_CONNECT_INIT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");

					break;

				case HTTP_MSG_WIFI_CONNECT_SUCCESS:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");

					break;

				case HTTP_MSG_WIFI_CONNECT_FAIL:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");

					break;

				case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
					g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
					http_server_fw_update_reset_timer();

					break;

				case HTTP_MSG_OTA_UPDATE_FAILED:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
					g_fw_update_status = OTA_UPDATE_FAILED;

					break;

				default:
					break;
			}
		}
	}
}

/**
 * Jquery get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_jquery_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "Jquery requested");

	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)jquery_3_3_1_min_js_start, jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);

	return ESP_OK;
}

/**
 * Sends the index.html page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "index.html requested");

	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

	return ESP_OK;
}

/**
 * app.css get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "app.css requested");

	httpd_resp_set_type(req, "text/css");
	httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start);

	return ESP_OK;
}

/**
 * app.js get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "app.js requested");

	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start);

	return ESP_OK;
}

/**
 * Sends the .ico (icon) file when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "favicon.ico requested");

	httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);

	return ESP_OK;
}

/**
 * Receives the .bin file fia the web page and handles the firmware update
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK, otherwise ESP_FAIL if timeout occurs and the update cannot be started.
 */
esp_err_t http_server_OTA_update_handler(httpd_req_t *req)
{
	esp_ota_handle_t ota_handle;

	char ota_buff[1024];
	int content_length = req->content_len;
	int content_received = 0;
	int recv_len;
	bool is_req_body_started = false;
	bool flash_successful = false;

	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

	do
	{
		// Read the data for the request
		if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0)
		{
			// Check if timeout occurred
			if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
			{
				ESP_LOGI(TAG, "http_server_OTA_update_handler: Socket Timeout");
				continue; ///> Retry receiving if timeout occurred
			}
			ESP_LOGI(TAG, "http_server_OTA_update_handler: OTA other Error %d", recv_len);
			return ESP_FAIL;
		}
		printf("http_server_OTA_update_handler: OTA RX: %d of %d\r", content_received, content_length);

		// Is this the first data we are receiving
		// If so, it will have the information in the header that we need.
		if (!is_req_body_started)
		{
			is_req_body_started = true;

			// Get the location of the .bin file content (remove the web form data)
			char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
			int body_part_len = recv_len - (body_start_p - ota_buff);

			printf("http_server_OTA_update_handler: OTA file size: %d\r\n", content_length);

			esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			if (err != ESP_OK)
			{
				printf("http_server_OTA_update_handler: Error with OTA begin, cancelling OTA\r\n");
				return ESP_FAIL;
			}
			else
			{
				//printf("http_server_OTA_update_handler: Writing to partition subtype %d at offset 0x%lx\r\n", update_partition->subtype, update_partition->address);
			}

			// Write this first part of the data
			esp_ota_write(ota_handle, body_start_p, body_part_len);
			content_received += body_part_len;
		}
		else
		{
			// Write OTA data
			esp_ota_write(ota_handle, ota_buff, recv_len);
			content_received += recv_len;
		}

	} while (recv_len > 0 && content_received < content_length);

	if (esp_ota_end(ota_handle) == ESP_OK)
	{
		// Lets update the partition
		if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
		{
			flash_successful = true;
		}
		else
		{
			ESP_LOGI(TAG, "http_server_OTA_update_handler: FLASHED ERROR!!!");
		}
	}
	else
	{
		ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end ERROR!!!");
	}

	// We won't update the global variables throughout the file, so send the message about the status
	if (flash_successful) { http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL); } else { http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED); }

	return ESP_OK;
}

/**
 * OTA status handler responds with the firmware update status after the OTA update is started
 * and responds with the compile time/date when the page is first requested
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
esp_err_t http_server_OTA_status_handler(httpd_req_t *req)
{
	char otaJSON[100];

	ESP_LOGI(TAG, "OTAstatus requested");

	sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", g_fw_update_status, __TIME__, __DATE__);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, otaJSON, strlen(otaJSON));

	return ESP_OK;
}

/**
 * DHT sensor readings JSON handler responds with DHT22 sensor data
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
static esp_err_t http_server_get_dht_sensor_readings_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/dhtSensor.json requested");

	char dhtSensorJSON[100];

	sprintf(dhtSensorJSON, "{\"temp\":\"%.1f\",\"humidity\":\"%.1f\"}", 30.1, 40.5);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, dhtSensorJSON, strlen(dhtSensorJSON));

	return ESP_OK;
}

static esp_err_t http_server_toogle_led_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/toogle_led.json requested");

	s_led_state = !s_led_state;
	gpio_set_level(BLINK_GPIO, s_led_state);

	// Cerrar la conexion
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);
    
    

	return ESP_OK;
}

// =====================================================
// http_server_set_config — stores pointer to system config
// =====================================================

void http_server_set_config(system_config_t *cfg)
{
	g_cfg = cfg;
}

// =====================================================
// Helper: send JSON response
// =====================================================

static void send_json(httpd_req_t *req, const char *json)
{
	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, json, strlen(json));
}

// =====================================================
// Handler: GET /api/status
// Returns full system status as JSON
// =====================================================

static esp_err_t api_status_handler(httpd_req_t *req)
{
	if (!g_cfg) { send_json(req, "{\"error\":\"config not ready\"}"); return ESP_OK; }

	xSemaphoreTake(g_cfg->mutex, portMAX_DELAY);

	float temp_c  = g_cfg->temperatura;
	int   unidad  = g_cfg->unidad;
	float pot_v  = g_cfg->pot_valor;
	int   pot_p  = g_cfg->pot_pct;
	int   modo   = g_cfg->modo;

	int   fan_modo  = g_cfg->fan_modo;
	float fan_td    = g_cfg->fan_temp_deseada;
	float fan_tm    = g_cfg->fan_temp_maxima;
	int   fan_mp    = g_cfg->fan_manual_pct;
	int   fan_duty  = g_cfg->fan_duty_actual;
	int   fan_alarm = g_cfg->fan_alarma;

	int   sv_modo = g_cfg->servo_modo;
	int   sv_mp   = g_cfg->servo_manual_pct;
	int   sv_pos  = g_cfg->servo_posicion_actual;

	float int_r = g_cfg->int_red;
	float int_g = g_cfg->int_green;
	float int_b = g_cfg->int_blue;
	int   intervalo = g_cfg->intervalo_ms;

	float brillo = g_cfg->rgb_brillo;

	xSemaphoreGive(g_cfg->mutex);

	float temp_conv = convertir_temperatura(temp_c, unidad);

	char buf[1024];
	snprintf(buf, sizeof(buf),
		"{"
		"\"temp\":%.2f,"
		"\"unidad\":%d,"
		"\"temp_sym\":\"%s\","
		"\"modo\":%d,"
		"\"pot_pct\":%d,"
		"\"pot_val\":%.2f,"
		"\"fan_modo\":%d,"
		"\"fan_temp_deseada\":%.1f,"
		"\"fan_temp_maxima\":%.1f,"
		"\"fan_manual_pct\":%d,"
		"\"fan_duty\":%d,"
		"\"fan_alarma\":%d,"
		"\"servo_modo\":%d,"
		"\"servo_manual_pct\":%d,"
		"\"servo_posicion\":%d,"
		"\"int_red\":%.2f,"
		"\"int_green\":%.2f,"
		"\"int_blue\":%.2f,"
		"\"brillo\":%.2f,"
		"\"intervalo_ms\":%d"
		"}",
		temp_conv, unidad, simbolo_unidad(unidad), modo, pot_p, pot_v,
		fan_modo, fan_td, fan_tm, fan_mp, fan_duty, fan_alarm,
		sv_modo, sv_mp, sv_pos,
		int_r, int_g, int_b, brillo, intervalo
	);

	send_json(req, buf);
	return ESP_OK;
}

// =====================================================
// Helper: parse JSON body from POST request
// =====================================================

static char *read_post_body(httpd_req_t *req)
{
	char *buf = NULL;
	int len = req->content_len;
	if (len <= 0) return NULL;
	buf = malloc(len + 1);
	if (!buf) return NULL;
	int ret = httpd_req_recv(req, buf, len);
	if (ret <= 0) { free(buf); return NULL; }
	buf[len] = '\0';
	return buf;
}

// =====================================================
// Handler: POST /api/fan
// Body: {"modo":0|1, "temp_deseada":25.0, "temp_maxima":35.0, "manual_pct":50}
// =====================================================

static esp_err_t api_fan_handler(httpd_req_t *req)
{
	if (!g_cfg) { send_json(req, "{\"error\":\"config not ready\"}"); return ESP_OK; }

	char *body = read_post_body(req);
	if (!body) { send_json(req, "{\"error\":\"no body\"}"); return ESP_OK; }

	cJSON *root = cJSON_Parse(body);
	free(body);
	if (!root) { send_json(req, "{\"error\":\"bad json\"}"); return ESP_OK; }

	cJSON *m = cJSON_GetObjectItem(root, "modo");
	cJSON *td = cJSON_GetObjectItem(root, "temp_deseada");
	cJSON *tm = cJSON_GetObjectItem(root, "temp_maxima");
	cJSON *mp = cJSON_GetObjectItem(root, "manual_pct");

	xSemaphoreTake(g_cfg->mutex, portMAX_DELAY);
	if (m && cJSON_IsNumber(m))  g_cfg->fan_modo = m->valueint;
	if (td && cJSON_IsNumber(td)) g_cfg->fan_temp_deseada = (float)td->valuedouble;
	if (tm && cJSON_IsNumber(tm)) g_cfg->fan_temp_maxima = (float)tm->valuedouble;
	if (mp && cJSON_IsNumber(mp)) g_cfg->fan_manual_pct = mp->valueint;
	xSemaphoreGive(g_cfg->mutex);

	cJSON_Delete(root);
	send_json(req, "{\"ok\":\"fan updated\"}");
	return ESP_OK;
}

// =====================================================
// Handler: POST /api/servo
// Body: {"modo":0|1, "manual_pct":50}
// =====================================================

static esp_err_t api_servo_handler(httpd_req_t *req)
{
	if (!g_cfg) { send_json(req, "{\"error\":\"config not ready\"}"); return ESP_OK; }

	char *body = read_post_body(req);
	if (!body) { send_json(req, "{\"error\":\"no body\"}"); return ESP_OK; }

	cJSON *root = cJSON_Parse(body);
	free(body);
	if (!root) { send_json(req, "{\"error\":\"bad json\"}"); return ESP_OK; }

	cJSON *m = cJSON_GetObjectItem(root, "modo");
	cJSON *mp = cJSON_GetObjectItem(root, "manual_pct");

	xSemaphoreTake(g_cfg->mutex, portMAX_DELAY);
	if (m && cJSON_IsNumber(m))  g_cfg->servo_modo = m->valueint;
	if (mp && cJSON_IsNumber(mp)) g_cfg->servo_manual_pct = mp->valueint;
	xSemaphoreGive(g_cfg->mutex);

	cJSON_Delete(root);
	send_json(req, "{\"ok\":\"servo updated\"}");
	return ESP_OK;
}

// =====================================================
// Handler: POST /api/servo/schedule
// Body: {"index":0, "hora":14, "minuto":30, "porcentaje":75, "activo":1}
// =====================================================

static esp_err_t api_servo_schedule_handler(httpd_req_t *req)
{
	if (!g_cfg) { send_json(req, "{\"error\":\"config not ready\"}"); return ESP_OK; }

	char *body = read_post_body(req);
	if (!body) { send_json(req, "{\"error\":\"no body\"}"); return ESP_OK; }

	cJSON *root = cJSON_Parse(body);
	free(body);
	if (!root) { send_json(req, "{\"error\":\"bad json\"}"); return ESP_OK; }

	cJSON *idx = cJSON_GetObjectItem(root, "index");
	cJSON *h   = cJSON_GetObjectItem(root, "hora");
	cJSON *min = cJSON_GetObjectItem(root, "minuto");
	cJSON *pct = cJSON_GetObjectItem(root, "porcentaje");
	cJSON *act = cJSON_GetObjectItem(root, "activo");

	if (idx && cJSON_IsNumber(idx))
	{
		int i = idx->valueint;
		if (i >= 0 && i < SERVO_MAX_SCHEDULES)
		{
			xSemaphoreTake(g_cfg->mutex, portMAX_DELAY);
			if (h && cJSON_IsNumber(h))   g_cfg->servo_horarios[i].hora = h->valueint;
			if (min && cJSON_IsNumber(min)) g_cfg->servo_horarios[i].minuto = min->valueint;
			if (pct && cJSON_IsNumber(pct)) g_cfg->servo_horarios[i].porcentaje = pct->valueint;
			if (act && cJSON_IsNumber(act)) g_cfg->servo_horarios[i].activo = act->valueint;
			xSemaphoreGive(g_cfg->mutex);
		}
	}

	cJSON_Delete(root);
	send_json(req, "{\"ok\":\"schedule updated\"}");
	return ESP_OK;
}

// =====================================================
// Handler: GET /api/servo/schedule
// Returns all schedule entries as JSON array
// =====================================================

static esp_err_t api_servo_schedule_get_handler(httpd_req_t *req)
{
	if (!g_cfg) { send_json(req, "{\"error\":\"config not ready\"}"); return ESP_OK; }

	char buf[1024];
	int off = 0;
	off += snprintf(buf + off, sizeof(buf) - off, "[");

	xSemaphoreTake(g_cfg->mutex, portMAX_DELAY);
	for (int i = 0; i < SERVO_MAX_SCHEDULES; i++)
	{
		servo_schedule_t *s = &g_cfg->servo_horarios[i];
		off += snprintf(buf + off, sizeof(buf) - off,
			"%s{\"index\":%d,\"hora\":%d,\"minuto\":%d,\"porcentaje\":%d,\"activo\":%d}",
			(i > 0) ? "," : "", i, s->hora, s->minuto, s->porcentaje, s->activo);
	}
	xSemaphoreGive(g_cfg->mutex);

	off += snprintf(buf + off, sizeof(buf) - off, "]");

	send_json(req, buf);
	return ESP_OK;
}

// =====================================================
// Handler: POST /api/rgb
// Body: {"red":0-255, "green":0-255, "blue":0-255, "brillo":0-100}
// =====================================================

static esp_err_t api_rgb_handler(httpd_req_t *req)
{
	if (!g_cfg) { send_json(req, "{\"error\":\"config not ready\"}"); return ESP_OK; }

	char *body = read_post_body(req);
	if (!body) { send_json(req, "{\"error\":\"no body\"}"); return ESP_OK; }

	cJSON *root = cJSON_Parse(body);
	free(body);
	if (!root) { send_json(req, "{\"error\":\"bad json\"}"); return ESP_OK; }

	cJSON *r = cJSON_GetObjectItem(root, "red");
	cJSON *g = cJSON_GetObjectItem(root, "green");
	cJSON *b = cJSON_GetObjectItem(root, "blue");
	cJSON *br = cJSON_GetObjectItem(root, "brillo");

	xSemaphoreTake(g_cfg->mutex, portMAX_DELAY);
	if (r && cJSON_IsNumber(r))  g_cfg->int_red = (float)(r->valueint) / 255.0f;
	if (g && cJSON_IsNumber(g))  g_cfg->int_green = (float)(g->valueint) / 255.0f;
	if (b && cJSON_IsNumber(b))  g_cfg->int_blue = (float)(b->valueint) / 255.0f;
	if (br && cJSON_IsNumber(br))
	{
		float bv = (float)br->valuedouble;
		if (bv < 0.0f) bv = 0.0f;
		if (bv > 100.0f) bv = 100.0f;
		g_cfg->rgb_brillo = bv / 100.0f;
	}
	xSemaphoreGive(g_cfg->mutex);

	cJSON_Delete(root);
	send_json(req, "{\"ok\":\"rgb updated\"}");
	return ESP_OK;
}

// =====================================================
// Handler: POST /api/wifi/sta
// Body: {"ssid":"...", "password":"..."}
// =====================================================

static esp_err_t api_wifi_sta_handler(httpd_req_t *req)
{
	if (!g_cfg) { send_json(req, "{\"error\":\"config not ready\"}"); return ESP_OK; }

	char *body = read_post_body(req);
	if (!body) { send_json(req, "{\"error\":\"no body\"}"); return ESP_OK; }

	cJSON *root = cJSON_Parse(body);
	free(body);
	if (!root) { send_json(req, "{\"error\":\"bad json\"}"); return ESP_OK; }

	cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
	cJSON *pass = cJSON_GetObjectItem(root, "password");

	if (ssid && cJSON_IsString(ssid) && pass && cJSON_IsString(pass))
	{
		xSemaphoreTake(g_cfg->mutex, portMAX_DELAY);
		strncpy(g_cfg->wifi_sta_ssid, ssid->valuestring, WIFI_SSID_MAX_LEN - 1);
		strncpy(g_cfg->wifi_sta_pass, pass->valuestring, WIFI_PASS_MAX_LEN - 1);
		g_cfg->wifi_sta_ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
		g_cfg->wifi_sta_pass[WIFI_PASS_MAX_LEN - 1] = '\0';
		xSemaphoreGive(g_cfg->mutex);

		wifi_app_save_sta_credentials(ssid->valuestring, pass->valuestring);
		wifi_app_send_message(WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT);
		vTaskDelay(pdMS_TO_TICKS(500));
		wifi_app_send_message(WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER);
	}

	cJSON_Delete(root);
	send_json(req, "{\"ok\":\"sta credentials saved\"}");
	return ESP_OK;
}

// =====================================================
// Handler: POST /api/wifi/ap
// Body: {"ssid":"...", "password":"..."}
// =====================================================

static esp_err_t api_wifi_ap_handler(httpd_req_t *req)
{
	if (!g_cfg) { send_json(req, "{\"error\":\"config not ready\"}"); return ESP_OK; }

	char *body = read_post_body(req);
	if (!body) { send_json(req, "{\"error\":\"no body\"}"); return ESP_OK; }

	cJSON *root = cJSON_Parse(body);
	free(body);
	if (!root) { send_json(req, "{\"error\":\"bad json\"}"); return ESP_OK; }

	cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
	cJSON *pass = cJSON_GetObjectItem(root, "password");

	if (ssid && cJSON_IsString(ssid) && pass && cJSON_IsString(pass))
	{
		xSemaphoreTake(g_cfg->mutex, portMAX_DELAY);
		strncpy(g_cfg->wifi_ap_ssid, ssid->valuestring, WIFI_SSID_MAX_LEN - 1);
		strncpy(g_cfg->wifi_ap_pass, pass->valuestring, WIFI_PASS_MAX_LEN - 1);
		g_cfg->wifi_ap_ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
		g_cfg->wifi_ap_pass[WIFI_PASS_MAX_LEN - 1] = '\0';
		xSemaphoreGive(g_cfg->mutex);

		wifi_config_t ap_cfg = { 0 };
		strncpy((char *)ap_cfg.ap.ssid, ssid->valuestring, sizeof(ap_cfg.ap.ssid) - 1);
		strncpy((char *)ap_cfg.ap.password, pass->valuestring, sizeof(ap_cfg.ap.password) - 1);
		ap_cfg.ap.ssid_len = strlen(ssid->valuestring);
		ap_cfg.ap.channel = WIFI_AP_CHANNEL;
		ap_cfg.ap.max_connection = WIFI_AP_MAX_CONN;
		ap_cfg.ap.authmode = (strlen(pass->valuestring) >= 8) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
		esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
	}

	cJSON_Delete(root);
	send_json(req, "{\"ok\":\"ap credentials saved\"}");
	return ESP_OK;
}


/**
 * Sets up the default httpd server configuration.
 * @return http server instance handle if successful, NULL otherwise.
 */
static httpd_handle_t http_server_configure(void)
{
	// Generate the default configuration
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	// Create the message queue
	http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));

	// Create HTTP server monitor task
	xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor", HTTP_SERVER_MONITOR_STACK_SIZE, NULL, HTTP_SERVER_MONITOR_PRIORITY, &task_http_server_monitor, HTTP_SERVER_MONITOR_CORE_ID);

	

	// The core that the HTTP server will run on
	config.core_id = HTTP_SERVER_TASK_CORE_ID;

	// Adjust the default priority to 1 less than the wifi application task
	config.task_priority = HTTP_SERVER_TASK_PRIORITY;

	// Bump up the stack size (default is 4096)
	config.stack_size = HTTP_SERVER_TASK_STACK_SIZE;

	// Increase uri handlers
	config.max_uri_handlers = 20;

	// Increase the timeout limits
	config.recv_wait_timeout = 10;
	config.send_wait_timeout = 10;

	ESP_LOGI(TAG,
			"http_server_configure: Starting server on port: '%d' with task priority: '%d'",
			config.server_port,
			config.task_priority);

	// Start the httpd server
	if (httpd_start(&http_server_handle, &config) == ESP_OK)
	{
		ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");

		// register index.html handler
		httpd_uri_t index_html = {
				.uri = "/",
				.method = HTTP_GET,
				.handler = http_server_index_html_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &index_html);

		// register query handler
		httpd_uri_t jquery_js = {
				.uri = "/jquery-3.3.1.min.js",
				.method = HTTP_GET,
				.handler = http_server_jquery_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &jquery_js);

		
		

		// register app.css handler
		httpd_uri_t app_css = {
				.uri = "/app.css",
				.method = HTTP_GET,
				.handler = http_server_app_css_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_css);

		// register app.js handler
		httpd_uri_t app_js = {
				.uri = "/app.js",
				.method = HTTP_GET,
				.handler = http_server_app_js_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_js);

		// register favicon.ico handler
		httpd_uri_t favicon_ico = {
				.uri = "/favicon.ico",
				.method = HTTP_GET,
				.handler = http_server_favicon_ico_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &favicon_ico);

		// register OTAupdate handler
		httpd_uri_t OTA_update = {
				.uri = "/OTAupdate",
				.method = HTTP_POST,
				.handler = http_server_OTA_update_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_update);

		// register OTAstatus handler
		httpd_uri_t OTA_status = {
				.uri = "/OTAstatus",
				.method = HTTP_POST,
				.handler = http_server_OTA_status_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_status);

		// register dhtSensor.json handler
		httpd_uri_t dht_sensor_json = {
				.uri = "/dhtSensor.json",
				.method = HTTP_GET,
				.handler = http_server_get_dht_sensor_readings_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &dht_sensor_json);
		
		// register toogle_led handler
		httpd_uri_t toogle_led = {
				.uri = "/toogle_led.json",
				.method = HTTP_POST,
				.handler = http_server_toogle_led_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &toogle_led);

		// =============================================
		// STR 2026 — API handlers
		// =============================================

		httpd_uri_t api_status = {
				.uri = "/api/status",
				.method = HTTP_GET,
				.handler = api_status_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &api_status);

		httpd_uri_t api_fan = {
				.uri = "/api/fan",
				.method = HTTP_POST,
				.handler = api_fan_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &api_fan);

		httpd_uri_t api_servo = {
				.uri = "/api/servo",
				.method = HTTP_POST,
				.handler = api_servo_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &api_servo);

		httpd_uri_t api_servo_sched = {
				.uri = "/api/servo/schedule",
				.method = HTTP_POST,
				.handler = api_servo_schedule_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &api_servo_sched);

		httpd_uri_t api_servo_sched_get = {
				.uri = "/api/servo/schedule",
				.method = HTTP_GET,
				.handler = api_servo_schedule_get_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &api_servo_sched_get);

		httpd_uri_t api_rgb = {
				.uri = "/api/rgb",
				.method = HTTP_POST,
				.handler = api_rgb_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &api_rgb);

		httpd_uri_t api_wifi_sta = {
				.uri = "/api/wifi/sta",
				.method = HTTP_POST,
				.handler = api_wifi_sta_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &api_wifi_sta);

		httpd_uri_t api_wifi_ap = {
				.uri = "/api/wifi/ap",
				.method = HTTP_POST,
				.handler = api_wifi_ap_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &api_wifi_ap);

		rgb_led_http_server_started();

		return http_server_handle;
	}

	return NULL;
}

void http_server_start(void)
{
	if (http_server_handle == NULL)
	{
		http_server_handle = http_server_configure();
	}
}

void http_server_stop(void)
{
	if (http_server_handle)
	{
		httpd_stop(http_server_handle);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
		http_server_handle = NULL;
	}
	if (task_http_server_monitor)
	{
		vTaskDelete(task_http_server_monitor);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
		task_http_server_monitor = NULL;
	}
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID)
{
	http_server_queue_message_t msg;
	msg.msgID = msgID;
	return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);
}

void http_server_fw_update_reset_callback(void *arg)
{
	ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the device");
	esp_restart();
}




















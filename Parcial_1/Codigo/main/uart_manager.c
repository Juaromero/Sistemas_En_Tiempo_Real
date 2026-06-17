#include "uart_manager.h"
#include "ntc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#define TAG "UART"

// =====================================================
// UART INIT
// =====================================================

void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    ESP_ERROR_CHECK(uart_driver_install(
        UART_PORT_NUM, UART_BUFFER_SIZE, 0, 0, NULL, 0
    ));

    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));

    ESP_LOGI(TAG, "UART inicializado a %d baud", UART_BAUD_RATE);
}

// =====================================================
// SEND — envia string con CRLF
// =====================================================

static void uart_send(const char *msg)
{
    uart_write_bytes(UART_PORT_NUM, msg, strlen(msg));
    uart_write_bytes(UART_PORT_NUM, "\r\n", 2);
}

// =====================================================
// Convierte valor en la unidad activa a Celsius
// =====================================================

static float a_celsius(float valor, int unidad)
{
    switch (unidad)
    {
        case UNIDAD_FAHRENHEIT: return (valor - 32.0f) * 5.0f / 9.0f;
        case UNIDAD_KELVIN:     return valor - 273.15f;
        case UNIDAD_CELSIUS:
        default:                return valor;
    }
}

// =====================================================
// PROCESS COMMAND
// =====================================================

static void process_command(char *cmd, system_config_t *cfg)
{
    // -------------------------------------------------
    // GET TEMP
    // -------------------------------------------------

    if (strcmp(cmd, "GET TEMP") == 0)
    {
        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        float temp_c = cfg->temperatura;
        int   unidad = cfg->unidad;
        xSemaphoreGive(cfg->mutex);

        float temp_conv = convertir_temperatura(temp_c, unidad);

        char resp[48];
        snprintf(resp, sizeof(resp), "TEMP %.2f %s",
                 temp_conv, simbolo_unidad(unidad));
        uart_send(resp);
        return;
    }

    // -------------------------------------------------
    // SET UNIT <C|F|K>
    // -------------------------------------------------

    if (strncmp(cmd, "SET UNIT ", 9) == 0)
    {
        const char *u = cmd + 9;
        int nueva_unidad = -1;

        if      (strcmp(u, "C") == 0) nueva_unidad = UNIDAD_CELSIUS;
        else if (strcmp(u, "F") == 0) nueva_unidad = UNIDAD_FAHRENHEIT;
        else if (strcmp(u, "K") == 0) nueva_unidad = UNIDAD_KELVIN;

        if (nueva_unidad == -1)
        {
            uart_send("ERROR unidad invalida. Usar: C F K");
            return;
        }

        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        cfg->unidad = nueva_unidad;
        xSemaphoreGive(cfg->mutex);

        char resp[32];
        snprintf(resp, sizeof(resp), "OK UNIT %s", simbolo_unidad(nueva_unidad));
        uart_send(resp);
        ESP_LOGI(TAG, "Unidad cambiada a %s", simbolo_unidad(nueva_unidad));
        return;
    }

    // -------------------------------------------------
    // SET INTERVAL <segundos>
    // -------------------------------------------------

    if (strncmp(cmd, "SET INTERVAL ", 13) == 0)
    {
        int segundos = atoi(cmd + 13);

        if (segundos < TEMP_INTERVAL_MIN || segundos > TEMP_INTERVAL_MAX)
        {
            char err[64];
            snprintf(err, sizeof(err),
                     "ERROR rango invalido. Usar: %d a %d segundos",
                     TEMP_INTERVAL_MIN, TEMP_INTERVAL_MAX);
            uart_send(err);
            return;
        }

        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        cfg->intervalo_ms = segundos * 1000;
        xSemaphoreGive(cfg->mutex);

        char resp[40];
        snprintf(resp, sizeof(resp), "OK INTERVAL %d s", segundos);
        uart_send(resp);
        ESP_LOGI(TAG, "Intervalo de temperatura: %d s", segundos);
        return;
    }

    // -------------------------------------------------
    // SET LIM <COLOR> <min> <max>   (Modo 2)
    // Los valores se reciben en la unidad activa y se
    // convierten a Celsius para almacenamiento interno.
    // -------------------------------------------------

    if (strncmp(cmd, "SET LIM ", 8) == 0)
    {
        char  color[16] = {0};
        float v1, v2;

        if (sscanf(cmd + 8, "%15s %f %f", color, &v1, &v2) != 3)
        {
            uart_send("ERROR formato: SET LIM <RED|GREEN|BLUE> <min> <max>");
            return;
        }

        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        int unidad = cfg->unidad;
        xSemaphoreGive(cfg->mutex);

        float c1 = a_celsius(v1, unidad);
        float c2 = a_celsius(v2, unidad);

        xSemaphoreTake(cfg->mutex, portMAX_DELAY);

        if (strcmp(color, "RED") == 0)
        {
            cfg->lim_red.min = c1; cfg->lim_red.max = c2;
            xSemaphoreGive(cfg->mutex);
            ESP_LOGI(TAG, "LIM RED: %.2f - %.2f C", c1, c2);
            uart_send("OK LIM RED");
        }
        else if (strcmp(color, "GREEN") == 0)
        {
            cfg->lim_green.min = c1; cfg->lim_green.max = c2;
            xSemaphoreGive(cfg->mutex);
            ESP_LOGI(TAG, "LIM GREEN: %.2f - %.2f C", c1, c2);
            uart_send("OK LIM GREEN");
        }
        else if (strcmp(color, "BLUE") == 0)
        {
            cfg->lim_blue.min = c1; cfg->lim_blue.max = c2;
            xSemaphoreGive(cfg->mutex);
            ESP_LOGI(TAG, "LIM BLUE: %.2f - %.2f C", c1, c2);
            uart_send("OK LIM BLUE");
        }
        else
        {
            xSemaphoreGive(cfg->mutex);
            uart_send("ERROR color invalido. Usar: RED GREEN BLUE");
        }
        return;
    }

    // -------------------------------------------------
    // SET INT <COLOR> <valor 0-100>   (Modo 3)
    // -------------------------------------------------

    if (strncmp(cmd, "SET INT ", 8) == 0)
    {
        char  color[16] = {0};
        float v1;

        if (sscanf(cmd + 8, "%15s %f", color, &v1) != 2)
        {
            uart_send("ERROR formato: SET INT <RED|GREEN|BLUE> <0-100>");
            return;
        }

        if (v1 < 0.0f)   v1 = 0.0f;
        if (v1 > 100.0f) v1 = 100.0f;
        float norm = v1 / 100.0f;

        xSemaphoreTake(cfg->mutex, portMAX_DELAY);

        if (strcmp(color, "RED") == 0)
        {
            cfg->int_red = norm;
            xSemaphoreGive(cfg->mutex);
            ESP_LOGI(TAG, "INT RED: %.0f%%", v1);
            uart_send("OK INT RED");
        }
        else if (strcmp(color, "GREEN") == 0)
        {
            cfg->int_green = norm;
            xSemaphoreGive(cfg->mutex);
            ESP_LOGI(TAG, "INT GREEN: %.0f%%", v1);
            uart_send("OK INT GREEN");
        }
        else if (strcmp(color, "BLUE") == 0)
        {
            cfg->int_blue = norm;
            xSemaphoreGive(cfg->mutex);
            ESP_LOGI(TAG, "INT BLUE: %.0f%%", v1);
            uart_send("OK INT BLUE");
        }
        else
        {
            xSemaphoreGive(cfg->mutex);
            uart_send("ERROR color invalido. Usar: RED GREEN BLUE");
        }
        return;
    }

    // -------------------------------------------------
    // Comando no reconocido
    // -------------------------------------------------

    uart_send("ERROR comando no reconocido");
    uart_send("Comandos disponibles:");
    uart_send("  GET TEMP                              -- temperatura actual (unidad activa)");
    uart_send("  SET UNIT <C|F|K>                      -- cambia unidad de temperatura");
    uart_send("  SET INTERVAL <1-60>                   -- intervalo de impresion (segundos)");
    uart_send("  SET LIM <RED|GREEN|BLUE> <min> <max>  -- rango temperatura (Modo 2)");
    uart_send("  SET INT <RED|GREEN|BLUE> <0-100>      -- intensidad manual  (Modo 3)");
}

// =====================================================
// UART TASK
// =====================================================

void uart_task(void *pvParameters)
{
    system_config_t *cfg = (system_config_t *)pvParameters;

    uint8_t data[128];

    while (1)
    {
        int len = uart_read_bytes(
            UART_PORT_NUM,
            data,
            sizeof(data) - 1,
            pdMS_TO_TICKS(100)
        );

        if (len > 0)
        {
            data[len] = '\0';

            char *pos;
            pos = strchr((char *)data, '\r'); if (pos) *pos = '\0';
            pos = strchr((char *)data, '\n'); if (pos) *pos = '\0';

            if (strlen((char *)data) == 0) continue;

            ESP_LOGI(TAG, "Recibido: \"%s\"", (char *)data);
            process_command((char *)data, cfg);
        }
    }
}
#include "uart_manager.h"

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
// PROCESS COMMAND
//
// MODO 2 — SET LIM <RED|GREEN|BLUE> <min> <max>
//   Configura rango de temperatura para encender un canal.
//   Solo tiene efecto en MODO_TEMP_UART.
//
// MODO 3 — SET INT <RED|GREEN|BLUE> <0-100>
//   Configura intensidad manual de un canal.
//   Solo tiene efecto en MODO_INT_UART.
//
// MODO 4 — GET COLOR
//   Retorna los valores del potenciometro guardados.
//   Disponible desde cualquier modo.
//
// GET TEMP
//   Retorna la temperatura actual leida por la NTC.
//   Disponible desde cualquier modo.
// =====================================================

static void process_command(char *cmd, system_config_t *cfg)
{
    // -------------------------------------------------
    // GET TEMP — temperatura actual
    // -------------------------------------------------

    if (strcmp(cmd, "GET TEMP") == 0)
    {
        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        float temp = cfg->temperatura;
        xSemaphoreGive(cfg->mutex);

        char resp[32];
        snprintf(resp, sizeof(resp), "TEMP %.2f C", temp);
        uart_send(resp);
        return;
    }

    // -------------------------------------------------
    // GET COLOR — valores del potenciometro (Modo 4)
    // -------------------------------------------------

    if (strcmp(cmd, "GET COLOR") == 0)
    {
        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        int   listos = cfg->pot_listos;
        float pr     = cfg->pot_red;
        float pg     = cfg->pot_green;
        float pb     = cfg->pot_blue;
        xSemaphoreGive(cfg->mutex);

        if (listos)
        {
            char resp[64];
            snprintf(resp, sizeof(resp),
                     "POT R:%.0f G:%.0f B:%.0f",
                     pr * 100.0f, pg * 100.0f, pb * 100.0f);
            uart_send(resp);
        }
        else
        {
            uart_send("POT no configurado aun");
        }
        return;
    }

    // -------------------------------------------------
    // SET LIM <COLOR> <min> <max>   (Modo 2)
    // Formato: "SET LIM RED 20 30"
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

        if (strcmp(color, "RED") == 0)
        {
            cfg->lim_red.min = v1;
            cfg->lim_red.max = v2;
            xSemaphoreGive(cfg->mutex);
            ESP_LOGI(TAG, "LIM RED: %.2f - %.2f", v1, v2);
            uart_send("OK LIM RED");
        }
        else if (strcmp(color, "GREEN") == 0)
        {
            cfg->lim_green.min = v1;
            cfg->lim_green.max = v2;
            xSemaphoreGive(cfg->mutex);
            ESP_LOGI(TAG, "LIM GREEN: %.2f - %.2f", v1, v2);
            uart_send("OK LIM GREEN");
        }
        else if (strcmp(color, "BLUE") == 0)
        {
            cfg->lim_blue.min = v1;
            cfg->lim_blue.max = v2;
            xSemaphoreGive(cfg->mutex);
            ESP_LOGI(TAG, "LIM BLUE: %.2f - %.2f", v1, v2);
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
    // Formato: "SET INT RED 75"
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
    uart_send("  GET TEMP                         -- temperatura actual (cualquier modo)");
    uart_send("  GET COLOR                        -- color del potenciometro (cualquier modo)");
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
            pos = strchr((char *)data, '\r');
            if (pos) *pos = '\0';
            pos = strchr((char *)data, '\n');
            if (pos) *pos = '\0';

            if (strlen((char *)data) == 0) continue;

            ESP_LOGI(TAG, "Recibido: \"%s\"", (char *)data);
            process_command((char *)data, cfg);
        }
    }
}
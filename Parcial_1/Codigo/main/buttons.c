#include "buttons.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#define TAG "BUTTONS"

// =====================================================
// BUTTONS INIT
// Pull-up interno: nivel 0 = presionado
// =====================================================

void buttons_init(void)
{
    gpio_config_t io = {
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN_MODO) | (1ULL << BTN_BOOT),
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_LOGI(TAG, "Botones inicializados (MODO=GPIO%d, BOOT=GPIO%d)",
             BTN_MODO, BTN_BOOT);
}

// =====================================================
// BUTTONS TASK
//
// BTN_MODO (GPIO4):
//   Cicla: 2 -> 3 -> 4 -> 2 -> ...
//
// BTN_BOOT (GPIO9):
//   Cicla unidad: C -> F -> K -> C
//   Disponible en cualquier modo.
// =====================================================

void buttons_task(void *pvParameters)
{
    system_config_t *cfg = (system_config_t *)pvParameters;

    int last_btn_modo = 1;
    int last_btn_boot = 1;

    const char *nombre_modo[]   = { "", "", "2-TEMP UART", "3-INT UART",
                                            "4-POT UMBRAL" };
    const char *nombre_unidad[] = { "Celsius (C)", "Fahrenheit (F)", "Kelvin (K)" };

    while (1)
    {
        // =================================================
        // BTN_MODO — cicla modos 2, 3, 4
        // =================================================

        int btn_modo = gpio_get_level(BTN_MODO);

        if (btn_modo == 0 && last_btn_modo == 1)
        {
            vTaskDelay(pdMS_TO_TICKS(50));  // debounce

            xSemaphoreTake(cfg->mutex, portMAX_DELAY);

            cfg->modo++;
            if (cfg->modo > MODO_MAX)
                cfg->modo = MODO_MIN;

            int modo_actual = cfg->modo;

            xSemaphoreGive(cfg->mutex);

            ESP_LOGI(TAG, "Modo: %s", nombre_modo[modo_actual]);
        }

        last_btn_modo = btn_modo;

        // =================================================
        // BTN_BOOT (GPIO9) — cicla unidad de temperatura
        // C (0) -> F (1) -> K (2) -> C (0)
        // =================================================

        int btn_boot = gpio_get_level(BTN_BOOT);

        if (btn_boot == 0 && last_btn_boot == 1)
        {
            vTaskDelay(pdMS_TO_TICKS(50));  // debounce

            xSemaphoreTake(cfg->mutex, portMAX_DELAY);

            cfg->unidad = (cfg->unidad + 1) % 3;
            int unidad_actual = cfg->unidad;

            xSemaphoreGive(cfg->mutex);

            ESP_LOGI(TAG, "Unidad de temperatura: %s", nombre_unidad[unidad_actual]);
        }

        last_btn_boot = btn_boot;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
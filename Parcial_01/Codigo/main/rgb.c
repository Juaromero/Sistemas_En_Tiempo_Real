#include "rgb.h"

#include "driver/ledc.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#define TAG "RGB"

// =====================================================
// RGB INIT
// =====================================================

void rgb_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0,
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz         = PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    int            pines[3]   = { R1,    G1,    B1    };
    ledc_channel_t canales[3] = { CH_R1, CH_G1, CH_B1 };

    for (int i = 0; i < 3; i++)
    {
        ledc_channel_config_t ch = {
            .gpio_num   = pines[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = canales[i],
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = 0,
            .hpoint     = 0
        };

        ESP_ERROR_CHECK(ledc_channel_config(&ch));
    }

    ESP_LOGI(TAG, "RGB inicializado");
}

// =====================================================
// SET RGB — r, g, b en rango 0.0 - 1.0
// =====================================================

void set_rgb(float r, float g, float b)
{
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    if (g < 0.0f) g = 0.0f;
    if (g > 1.0f) g = 1.0f;
    if (b < 0.0f) b = 0.0f;
    if (b > 1.0f) b = 1.0f;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, CH_R1, (int)(r * PWM_MAX_DUTY));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, CH_R1);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, CH_G1, (int)(g * PWM_MAX_DUTY));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, CH_G1);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, CH_B1, (int)(b * PWM_MAX_DUTY));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, CH_B1);
}

// =====================================================
// RGB TASK — actualiza el LED cada 100ms
// =====================================================

void rgb_task(void *pvParameters)
{
    system_config_t *cfg = (system_config_t *)pvParameters;

    while (1)
    {
        xSemaphoreTake(cfg->mutex, portMAX_DELAY);

        int   modo    = cfg->modo;
        float temp    = cfg->temperatura;   // siempre en Celsius

        rango_t lim_r = cfg->lim_red;
        rango_t lim_g = cfg->lim_green;
        rango_t lim_b = cfg->lim_blue;

        float int_r   = cfg->int_red;
        float int_g   = cfg->int_green;
        float int_b   = cfg->int_blue;

        int   pot_pct = cfg->pot_pct;

        xSemaphoreGive(cfg->mutex);

        // =================================================
        // MODO 2 — Temperatura con rangos configurables
        // Comparacion interna siempre en Celsius
        // =================================================

        if (modo == MODO_TEMP_UART)
        {
            float r = (temp >= lim_r.min && temp <= lim_r.max) ? 1.0f : 0.0f;
            float g = (temp >= lim_g.min && temp <= lim_g.max) ? 1.0f : 0.0f;
            float b = (temp >= lim_b.min && temp <= lim_b.max) ? 1.0f : 0.0f;
            set_rgb(r, g, b);
        }

        // =================================================
        // MODO 3 — Intensidad manual por UART
        // =================================================

        else if (modo == MODO_INT_UART)
        {
            set_rgb(int_r, int_g, int_b);
        }

        // =================================================
        // MODO 4 — Potenciometro con umbrales de color
        //   0  .. UMBRAL_ROJO  (0-33%)  -> ROJO
        //   34 .. UMBRAL_VERDE (34-66%) -> VERDE
        //   67 .. 100          (67-100%)-> AZUL
        // =================================================

        else if (modo == MODO_POT_UMBRAL)
        {
            if (pot_pct <= UMBRAL_ROJO)
                set_rgb(1.0f, 0.0f, 0.0f);
            else if (pot_pct <= UMBRAL_VERDE)
                set_rgb(0.0f, 1.0f, 0.0f);
            else
                set_rgb(0.0f, 0.0f, 1.0f);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
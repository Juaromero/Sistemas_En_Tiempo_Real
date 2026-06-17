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
// SET RGB
// r, g, b en rango 0.0 - 1.0
// Catodo comun: duty alto = mas brillo
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
//
// MODO 1 — TEMP FIJO:
//   Rangos hardcodeados en config.h (TEMP_FRIO / TEMP_CALIENTE)
//   < TEMP_FRIO          -> AZUL  (100%)
//   TEMP_FRIO..CALIENTE  -> VERDE (100%)
//   > TEMP_CALIENTE      -> ROJO  (100%)
//
// MODO 2 — TEMP UART:
//   Igual pero con rangos lim_red/green/blue configurables
//   por UART con "SET LIM <COLOR> <min> <max>"
//   Cada canal se enciende al 100% si temp esta en su rango.
//
// MODO 3 — INT UART:
//   Intensidad de cada canal segun "SET INT <COLOR> <0-100>"
//
// MODO 4 — POTENCIOMETRO:
//   Mientras configura (pot_listos=0):
//     POT_CONFIG_RED   -> preview rojo en tiempo real
//     POT_CONFIG_BLUE  -> preview azul en tiempo real
//     POT_CONFIG_GREEN -> preview verde en tiempo real
//   Una vez guardados los 3 (pot_listos=1):
//     Muestra la combinacion completa guardada
// =====================================================

void rgb_task(void *pvParameters)
{
    system_config_t *cfg = (system_config_t *)pvParameters;

    while (1)
    {
        xSemaphoreTake(cfg->mutex, portMAX_DELAY);

        int   modo    = cfg->modo;
        float temp    = cfg->temperatura;

        rango_t lim_r = cfg->lim_red;
        rango_t lim_g = cfg->lim_green;
        rango_t lim_b = cfg->lim_blue;

        float int_r   = cfg->int_red;
        float int_g   = cfg->int_green;
        float int_b   = cfg->int_blue;

        float pot_r   = cfg->pot_red;
        float pot_g   = cfg->pot_green;
        float pot_b   = cfg->pot_blue;
        int   listos  = cfg->pot_listos;
        int   pot_est = cfg->pot_estado;
        float pot_val = cfg->pot_valor;

        xSemaphoreGive(cfg->mutex);

        // =================================================
        // MODO 1 — Temperatura con rangos fijos
        // =================================================

        if (modo == MODO_TEMP_FIJO)
        {
            if (temp < TEMP_FRIO)
            {
                set_rgb(0.0f, 0.0f, 1.0f);     // AZUL — frio
            }
            else if (temp <= TEMP_CALIENTE)
            {
                set_rgb(0.0f, 1.0f, 0.0f);     // VERDE — normal
            }
            else
            {
                set_rgb(1.0f, 0.0f, 0.0f);     // ROJO — caliente
            }
        }

        // =================================================
        // MODO 2 — Temperatura con rangos configurables
        // Cada canal independiente: se enciende si temp
        // esta dentro de su rango, sin importar los otros
        // =================================================

        else if (modo == MODO_TEMP_UART)
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
        // MODO 4 — Potenciometro
        // =================================================

        else if (modo == MODO_POTENCIOMETRO)
        {
            if (listos)
            {
                set_rgb(pot_r, pot_g, pot_b);
            }
            else
            {
                switch (pot_est)
                {
                    case POT_CONFIG_RED:
                        set_rgb(pot_val, 0.0f, 0.0f);
                        break;

                    case POT_CONFIG_BLUE:
                        set_rgb(0.0f, 0.0f, pot_val);
                        break;

                    case POT_CONFIG_GREEN:
                        set_rgb(0.0f, pot_val, 0.0f);
                        break;

                    default:
                        set_rgb(0.0f, 0.0f, 0.0f);
                        break;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
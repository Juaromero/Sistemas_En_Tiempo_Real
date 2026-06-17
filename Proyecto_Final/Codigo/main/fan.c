#include "fan.h"

#include "driver/ledc.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#define TAG "FAN"

// =====================================================
// FAN INIT
// =====================================================

void fan_init(void)
{
    // --- Timer LEDC para el fan ---
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = FAN_LEDC_TIMER,
        .duty_resolution = FAN_PWM_RESOLUTION,
        .freq_hz         = FAN_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_USE_XTAL_CLK
    };

    esp_err_t timer_err = ledc_timer_config(&timer);
    if (timer_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Timer LEDC no disponible para %dHz/%d bits, el ventilador no funcionara",
                 FAN_PWM_FREQ_HZ, 10);
        // [FIX] Si el timer falla no tiene sentido configurar el canal;
        //       retornamos para evitar una llamada ESP_ERROR_CHECK que abortaría.
        return;
    }

    // --- Canal LEDC para GPIO del fan ---
    ledc_channel_config_t ch = {
        .gpio_num   = FAN_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = FAN_LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = FAN_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };

    ESP_ERROR_CHECK(ledc_channel_config(&ch));

    // --- LED alarma — GPIO salida ---
    gpio_config_t io = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_ALARM_GPIO),
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(LED_ALARM_GPIO, 0);  // apagado al arranque

    ESP_LOGI(TAG, "Fan inicializado (GPIO%d, LEDC CH%d) | Alarma GPIO%d",
             FAN_GPIO, FAN_LEDC_CHANNEL, LED_ALARM_GPIO);
}

// =====================================================
// FAN SET SPEED — 0 a 100%
// =====================================================

void fan_set_speed(int porcentaje)
{
    if (porcentaje < 0)   porcentaje = 0;
    if (porcentaje > 100) porcentaje = 100;

    int duty = (int)((porcentaje / 100.0f) * FAN_PWM_MAX_DUTY);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, FAN_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, FAN_LEDC_CHANNEL);
}

// =====================================================
// FAN TASK
//
// Modo automatico (fan_modo == 0):
//   temp <= temp_deseada  ->  0%
//   temp >= temp_maxima   ->  100%  + activa alarma
//   entre ambas           ->  proporcional lineal
//
// Modo manual (fan_modo == 1):
//   aplica fan_manual_pct directamente
//   alarma activa si temp > temp_maxima independientemente
//
// Alarma LED 1 Hz: toggle cada 500ms cuando fan_alarma == 1
// =====================================================

void fan_task(void *pvParameters)
{
    system_config_t *cfg = (system_config_t *)pvParameters;

    int alarma_toggle = 0;
    int tick_alarma   = 0;

    while (1)
    {
        xSemaphoreTake(cfg->mutex, portMAX_DELAY);

        float temp       = cfg->temperatura;
        int   modo       = cfg->fan_modo;
        float t_deseada  = cfg->fan_temp_deseada;
        float t_maxima   = cfg->fan_temp_maxima;
        int   manual_pct = cfg->fan_manual_pct;

        xSemaphoreGive(cfg->mutex);

        // --- Calcular velocidad del fan ---
        int velocidad = 0;

        if (modo == 0)  // automatico
        {
            if (temp <= t_deseada)
            {
                velocidad = 0;
            }
            else if (temp >= t_maxima)
            {
                velocidad = 100;
            }
            else
            {
                float rango = t_maxima - t_deseada;
                if (rango < 0.1f) rango = 0.1f;
                velocidad = (int)(((temp - t_deseada) / rango) * 100.0f);
            }
        }
        else            // manual
        {
            velocidad = manual_pct;
        }

        fan_set_speed(velocidad);

        // --- Alarma: activa si temp supera maxima ---
        int alarma = (temp > t_maxima) ? 1 : 0;

        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        cfg->fan_alarma      = alarma;
        cfg->fan_duty_actual = velocidad;
        xSemaphoreGive(cfg->mutex);

        // --- Parpadeo LED alarma 1 Hz (toggle cada 500 ms = 5 ciclos de 100 ms) ---
        if (alarma)
        {
            tick_alarma++;
            if (tick_alarma >= 5)
            {
                tick_alarma   = 0;
                alarma_toggle = !alarma_toggle;
                gpio_set_level(LED_ALARM_GPIO, alarma_toggle);
            }
        }
        else
        {
            tick_alarma   = 0;
            alarma_toggle = 0;
            gpio_set_level(LED_ALARM_GPIO, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#include "rgb.h"

#include "driver/ledc.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#define TAG "RGB"

static system_config_t *rgb_cfg = NULL;

void rgb_set_cfg(system_config_t *cfg)
{
    rgb_cfg = cfg;
}

// =====================================================
// RGB STATUS — indicadores de estado WiFi/HTTP
// =====================================================

void rgb_wifi_app_started(void)
{
    if (!rgb_cfg) return;
    xSemaphoreTake(rgb_cfg->mutex, portMAX_DELAY);
    rgb_cfg->int_red   = 1.0f;
    rgb_cfg->int_green = 0.4f;
    rgb_cfg->int_blue  = 1.0f;
    xSemaphoreGive(rgb_cfg->mutex);
}

void rgb_http_server_started(void)
{
    if (!rgb_cfg) return;
    xSemaphoreTake(rgb_cfg->mutex, portMAX_DELAY);
    rgb_cfg->int_red   = 0.8f;
    rgb_cfg->int_green = 1.0f;
    rgb_cfg->int_blue  = 0.2f;
    xSemaphoreGive(rgb_cfg->mutex);
}

void rgb_wifi_connected(void)
{
    if (!rgb_cfg) return;
    xSemaphoreTake(rgb_cfg->mutex, portMAX_DELAY);
    rgb_cfg->int_red   = 0.0f;
    rgb_cfg->int_green = 1.0f;
    rgb_cfg->int_blue  = 0.6f;
    xSemaphoreGive(rgb_cfg->mutex);
}

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
        .clk_cfg         = LEDC_USE_XTAL_CLK
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
// RGB TASK — color y brillo directo desde config web
// =====================================================

void rgb_task(void *pvParameters)
{
    system_config_t *cfg = (system_config_t *)pvParameters;

    while (1)
    {
        xSemaphoreTake(cfg->mutex, portMAX_DELAY);

        float r = cfg->int_red;
        float g = cfg->int_green;
        float b = cfg->int_blue;
        float br = cfg->rgb_brillo;

        xSemaphoreGive(cfg->mutex);

        // Aplicar brillo global sobre cada canal
        set_rgb(r * br, g * br, b * br);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
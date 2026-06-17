#include "servo.h"

#include "driver/ledc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_sntp.h"

#include <time.h>
#include <string.h>

#define TAG "SERVO"

// =====================================================
// SERVO INIT
// =====================================================

void servo_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = SERVO_LEDC_TIMER,
        .duty_resolution = SERVO_PWM_RESOLUTION,
        .freq_hz         = SERVO_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_USE_XTAL_CLK
    };

    esp_err_t timer_err = ledc_timer_config(&timer);
    if (timer_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Timer LEDC no disponible para %dHz/%d bits, servo no funcionara",
                 SERVO_PWM_FREQ_HZ, 14);
        // [FIX] Igual que fan_init: no configurar el canal si el timer falló
        return;
    }

    ledc_channel_config_t ch = {
        .gpio_num   = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = SERVO_LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = SERVO_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };

    ESP_ERROR_CHECK(ledc_channel_config(&ch));

    servo_set_position(0);  // cierra cortina al arranque

    ESP_LOGI(TAG, "Servo inicializado (GPIO%d, LEDC CH%d, 50Hz)",
             SERVO_GPIO, SERVO_LEDC_CHANNEL);
}

// =====================================================
// SERVO SET POSITION — 0 a 100%
// =====================================================

void servo_set_position(int porcentaje)
{
    if (porcentaje < 0)   porcentaje = 0;
    if (porcentaje > 100) porcentaje = 100;

    float pulso_us = SERVO_PULSE_MIN_US
                   + (porcentaje / 100.0f)
                   * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US);

    // periodo = 1 / 50 Hz = 20000 us
    int duty = (int)((pulso_us / 20000.0f) * SERVO_PWM_MAX_DUTY);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_LEDC_CHANNEL);
}

// =====================================================
// SERVO TASK
// =====================================================

void servo_task(void *pvParameters)
{
    system_config_t *cfg = (system_config_t *)pvParameters;

    int ultimo_minuto_ejecutado = -1;

    while (1)
    {
        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        int modo       = cfg->servo_modo;
        int manual_pct = cfg->servo_manual_pct;
        xSemaphoreGive(cfg->mutex);

        if (modo == 1)  // manual
        {
            servo_set_position(manual_pct);

            xSemaphoreTake(cfg->mutex, portMAX_DELAY);
            cfg->servo_posicion_actual = manual_pct;
            xSemaphoreGive(cfg->mutex);
        }
        else            // automatico por horario
        {
            time_t    now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);

            int hora_actual   = timeinfo.tm_hour;
            int minuto_actual = timeinfo.tm_min;
            int clave         = hora_actual * 60 + minuto_actual;

            if (clave != ultimo_minuto_ejecutado)
            {
                // [FIX] En el original se hacía Give + Take DENTRO del for,
                //       dejando el mutex en un estado indefinido si se hacía
                //       break con el mutex tomado en algunos caminos.
                //       Ahora: tomamos el mutex una sola vez, copiamos los datos
                //       necesarios y lo soltamos; luego actuamos fuera del mutex.

                int    match_pct = -1;  // -1 = sin coincidencia

                xSemaphoreTake(cfg->mutex, portMAX_DELAY);

                for (int i = 0; i < SERVO_MAX_SCHEDULES; i++)
                {
                    servo_schedule_t *s = &cfg->servo_horarios[i];

                    if (s->activo
                        && s->hora   == hora_actual
                        && s->minuto == minuto_actual)
                    {
                        match_pct = s->porcentaje;
                        break;
                    }
                }

                if (match_pct >= 0)
                {
                    cfg->servo_posicion_actual = match_pct;
                }

                xSemaphoreGive(cfg->mutex);

                // Actuar FUERA del mutex (ledc_set_duty no necesita protección)
                if (match_pct >= 0)
                {
                    servo_set_position(match_pct);
                    ultimo_minuto_ejecutado = clave;
                    ESP_LOGI(TAG, "Horario ejecutado: %02d:%02d -> %d%%",
                             hora_actual, minuto_actual, match_pct);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
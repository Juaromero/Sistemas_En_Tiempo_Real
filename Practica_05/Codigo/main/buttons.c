#include "buttons.h"
#include "ntc.h"

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
        .pin_bit_mask = (1ULL << BTN_MODO) | (1ULL << BTN_GUARDAR),
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_LOGI(TAG, "Botones inicializados");
}

// =====================================================
// HELPER: resetear estado interno del potenciometro
// =====================================================

static void reset_pot(system_config_t *cfg)
{
    cfg->pot_estado = POT_CONFIG_RED;
    cfg->pot_listos = 0;
    cfg->pot_red    = 0.0f;
    cfg->pot_green  = 0.0f;
    cfg->pot_blue   = 0.0f;
    cfg->pot_valor  = 0.0f;
}

// =====================================================
// BUTTONS TASK
//
// BTN_MODO (GPIO4):
//   Cicla: 1 -> 2 -> 3 -> 4 -> 1 -> ...
//   Al entrar al modo 4 reinicia el estado del pot.
//
// BTN_GUARDAR (GPIO5):
//   Solo activo en MODO_POTENCIOMETRO (modo 4).
//   Ciclo de guardado:
//     POT_CONFIG_RED   -> guarda rojo,  pasa a POT_CONFIG_BLUE
//     POT_CONFIG_BLUE  -> guarda azul,  pasa a POT_CONFIG_GREEN
//     POT_CONFIG_GREEN -> guarda verde, pasa a POT_LISTO
//                         (pot_listos=1, LED muestra combinacion)
//     POT_LISTO        -> reinicia todo el ciclo
//
//   cfg->pot_valor se actualiza en tiempo real para que
//   rgb_task pueda mostrar preview del canal actual.
// =====================================================

void buttons_task(void *pvParameters)
{
    system_config_t *cfg = (system_config_t *)pvParameters;

    int last_btn_modo    = 1;
    int last_btn_guardar = 1;

    const char *nombre_estado[] = { "ROJO", "AZUL", "VERDE", "LISTO" };
    const char *nombre_modo[]   = { "", "1-TEMP FIJO", "2-TEMP UART",
                                       "3-INT UART",   "4-POTENCIOMETRO" };

    while (1)
    {
        // =================================================
        // Leer modo actual
        // =================================================

        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        int modo_actual = cfg->modo;
        xSemaphoreGive(cfg->mutex);

        // =================================================
        // BTN_MODO — cicla entre los 4 modos (1 a 4)
        // =================================================

        int btn_modo = gpio_get_level(BTN_MODO);

        if (btn_modo == 0 && last_btn_modo == 1)
        {
            vTaskDelay(pdMS_TO_TICKS(50));  // debounce

            xSemaphoreTake(cfg->mutex, portMAX_DELAY);

            cfg->modo++;
            if (cfg->modo > MODO_MAX)
            {
                cfg->modo = MODO_MIN;
            }

            if (cfg->modo == MODO_POTENCIOMETRO)
            {
                reset_pot(cfg);
            }

            modo_actual = cfg->modo;

            xSemaphoreGive(cfg->mutex);

            ESP_LOGI(TAG, "Modo: %s", nombre_modo[modo_actual]);
        }

        last_btn_modo = btn_modo;

        // =================================================
        // MODO 4 — Lectura continua del pot + BTN_GUARDAR
        // =================================================

        if (modo_actual == MODO_POTENCIOMETRO)
        {
            int   mv    = leer_adc_mv(ADC_POT_CHANNEL);
            float valor = mv / 3300.0f;
            if (valor > 1.0f) valor = 1.0f;
            if (valor < 0.0f) valor = 0.0f;

            xSemaphoreTake(cfg->mutex, portMAX_DELAY);
            cfg->pot_valor       = valor;
            int   est_local      = cfg->pot_estado;
            int   listos_local   = cfg->pot_listos;
            xSemaphoreGive(cfg->mutex);

            int idx = (est_local <= POT_LISTO) ? est_local : POT_LISTO;

            if (!listos_local)
            {
                ESP_LOGI(TAG, "Configurando %s | Pot: %.0f%%",
                         nombre_estado[idx], valor * 100.0f);
            }
            else
            {
                ESP_LOGI(TAG, "Color listo | Pot: %.0f%% (GUARDAR para reiniciar)",
                         valor * 100.0f);
            }

            // -----------------------------------------
            // BTN_GUARDAR
            // -----------------------------------------

            int btn_guardar = gpio_get_level(BTN_GUARDAR);

            if (btn_guardar == 0 && last_btn_guardar == 1)
            {
                vTaskDelay(pdMS_TO_TICKS(50));  // debounce

                xSemaphoreTake(cfg->mutex, portMAX_DELAY);

                int   est = cfg->pot_estado;
                float v   = cfg->pot_valor;

                if (est == POT_CONFIG_RED)
                {
                    cfg->pot_red    = v;
                    cfg->pot_estado = POT_CONFIG_BLUE;
                    ESP_LOGI(TAG, "Rojo guardado: %.0f%%", v * 100.0f);
                }
                else if (est == POT_CONFIG_BLUE)
                {
                    cfg->pot_blue   = v;
                    cfg->pot_estado = POT_CONFIG_GREEN;
                    ESP_LOGI(TAG, "Azul guardado: %.0f%%", v * 100.0f);
                }
                else if (est == POT_CONFIG_GREEN)
                {
                    cfg->pot_green  = v;
                    cfg->pot_estado = POT_LISTO;
                    cfg->pot_listos = 1;
                    ESP_LOGI(TAG, "Verde guardado: %.0f%%", v * 100.0f);
                    ESP_LOGI(TAG,
                             "Combinacion lista -> R:%.0f%% A:%.0f%% V:%.0f%%",
                             cfg->pot_red   * 100.0f,
                             cfg->pot_blue  * 100.0f,
                             cfg->pot_green * 100.0f);
                }
                else if (est == POT_LISTO)
                {
                    reset_pot(cfg);
                    ESP_LOGI(TAG, "Potenciometro reiniciado");
                }

                xSemaphoreGive(cfg->mutex);
            }

            last_btn_guardar = btn_guardar;
        }

        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
#include "config.h"

#include "ntc.h"
#include "rgb.h"
#include "buttons.h"
#include "uart_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// =====================================================
// APP MAIN
// =====================================================

void app_main(void)
{
    static system_config_t config =
    {
        // Modo 2 — rangos de temperatura configurables por UART
        // Valores iniciales identicos a los fijos del Modo 1
        .lim_red   = { .min = TEMP_CALIENTE, .max = 60.0f  },
        .lim_green = { .min = TEMP_FRIO,     .max = TEMP_CALIENTE },
        .lim_blue  = { .min =  0.0f,         .max = TEMP_FRIO     },

        // Modo 3 — intensidad inicial apagada
        .int_red   = 0.0f,
        .int_green = 0.0f,
        .int_blue  = 0.0f,

        // Modo 4 — potenciometro
        .pot_red    = 0.0f,
        .pot_green  = 0.0f,
        .pot_blue   = 0.0f,
        .pot_estado = POT_CONFIG_RED,
        .pot_listos = 0,
        .pot_valor  = 0.0f,

        // Temperatura y modo inicial
        .temperatura = 0.0f,
        .modo        = MODO_MIN
    };

    config.mutex = xSemaphoreCreateMutex();

    uart_init();
    rgb_init();
    adc_init();
    buttons_init();

    // Prioridades:
    //   uart_task        — 6  (procesa comandos del usuario)
    //   buttons_task     — 4  (responde a pulsaciones)
    //   rgb_task         — 3  (actualiza LED)
    //   temperature_task — 3  (lee sensor NTC)

    xTaskCreate(uart_task,        "uart_task",        4096, &config, 6, NULL);
    xTaskCreate(buttons_task,     "buttons_task",     4096, &config, 4, NULL);
    xTaskCreate(rgb_task,         "rgb_task",         4096, &config, 3, NULL);
    xTaskCreate(temperature_task, "temperature_task", 4096, &config, 3, NULL);
}
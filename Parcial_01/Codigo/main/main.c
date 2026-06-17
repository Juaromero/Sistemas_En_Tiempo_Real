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
        // Modo 2 — rangos de temperatura configurables por UART (en Celsius)
        .lim_red   = { .min = 35.0f, .max = 60.0f },
        .lim_green = { .min = 25.0f, .max = 35.0f },
        .lim_blue  = { .min =  0.0f, .max = 25.0f },

        // Modo 3 — intensidad inicial apagada
        .int_red   = 0.0f,
        .int_green = 0.0f,
        .int_blue  = 0.0f,

        // Modo 4 — potenciometro
        .pot_valor = 0.0f,
        .pot_pct   = 0,

        // Configuracion general
        .temperatura  = 0.0f,
        .unidad       = UNIDAD_CELSIUS, // arranca en Celsius
        .intervalo_ms = 500,            // imprime cada 500ms por defecto
        .modo         = MODO_MIN        // arranca en modo 2
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
    //   temperature_task — 3  (lee NTC y pot, imprime segun intervalo)

    xTaskCreate(uart_task,        "uart_task",        4096, &config, 6, NULL);
    xTaskCreate(buttons_task,     "buttons_task",     4096, &config, 4, NULL);
    xTaskCreate(rgb_task,         "rgb_task",         4096, &config, 3, NULL);
    xTaskCreate(temperature_task, "temperature_task", 4096, &config, 3, NULL);
}
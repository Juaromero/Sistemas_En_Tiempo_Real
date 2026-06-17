#include "config.h"

#include "ntc.h"
#include "rgb.h"
#include "fan.h"
#include "servo.h"

#include "http_server.h"
#include "wifi_app.h"
#include "rgb_led.h"

#include "nvs_flash.h"
#include "esp_log.h"          // [FIX] ESP_ERROR_CHECK/ESP_LOGI requieren este include

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// =====================================================
// APP MAIN
// =====================================================

void app_main(void)
{
    // Initialize NVS (needed for WiFi credentials storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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
        .unidad       = UNIDAD_CELSIUS,
        .intervalo_ms = 500,
        .modo         = MODO_MIN,

        // Fan: arranca en automatico, 25C deseada, 35C maxima
        .fan_modo          = 0,
        .fan_temp_deseada  = 25.0f,
        .fan_temp_maxima   = 35.0f,
        .fan_manual_pct    = 0,
        .fan_duty_actual   = 0,
        .fan_alarma        = 0,

        // Servo: arranca en manual, cerrado
        .servo_modo            = 1,
        .servo_manual_pct      = 0,
        .servo_posicion_actual = 0,

        // [FIX] servo_horarios no se puede inicializar con {0} en un designator
        // anidado en un static struct; queda zero-initializado por ser static.

        // RGB: brillo al 100%
        .rgb_brillo = 1.0f,

        // WiFi STA defaults
        .wifi_sta_ssid = WIFI_STA_SSID_DEFAULT,
        .wifi_sta_pass = WIFI_STA_PASS_DEFAULT,

        // WiFi AP defaults
        .wifi_ap_ssid = WIFI_AP_SSID_DEFAULT,
        .wifi_ap_pass = WIFI_AP_PASS_DEFAULT,

        // [FIX] mutex NO se puede inicializar aquí con un designator porque
        // xSemaphoreCreateMutex() es una llamada en tiempo de ejecución.
        // Se deja en NULL y se asigna justo debajo, ANTES de crear cualquier tarea.
        .mutex = NULL,
    };

    // [FIX] Crear el mutex ANTES de cualquier tarea o driver que lo use
    config.mutex = xSemaphoreCreateMutex();
    configASSERT(config.mutex != NULL);

    // Initialize board LED (GPIO for blink)
    // BLINK_GPIO está definido en http_server.h (valor 10); no se redefine aquí.
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    // Initialize hardware
    rgb_init();
    rgb_set_cfg(&config);
    adc_init();
    fan_init();
    servo_init();

    // Give HTTP server access to the config
    http_server_set_config(&config);

    // Start tasks
    xTaskCreate(rgb_task,         "rgb_task",         4096, &config, 3, NULL);
    xTaskCreate(temperature_task, "temperature_task", 4096, &config, 3, NULL);
    xTaskCreate(fan_task,         "fan_task",         4096, &config, 3, NULL);
    xTaskCreate(servo_task,       "servo_task",       4096, &config, 2, NULL);

    // Initialize NTP time synchronization state
    init_obtain_time();

    // Start WiFi (AP+STA) and HTTP server
    wifi_app_start();
}
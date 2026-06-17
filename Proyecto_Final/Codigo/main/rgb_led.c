/*
 * rgb_led.c
 *
 * LED RGB de estado WiFi — control por GPIO directo on/off.
 * Sin PWM, sin LEDC. Solo 3 pines digitales.
 *
 * Colores (catodo comun: HIGH = encendido):
 *   WiFi iniciando       -> morado  (R=1, G=0, B=1)
 *   Servidor HTTP listo  -> amarillo(R=1, G=1, B=0)
 *   WiFi conectado        -> cyan    (R=0, G=1, B=1)
 */

#include "rgb_led.h"
#include "driver/gpio.h"

static bool g_initialized = false;

void rgb_led_init(void)
{
    gpio_config_t io = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RGB_LED_RED_GPIO)
                      | (1ULL << RGB_LED_GREEN_GPIO)
                      | (1ULL << RGB_LED_BLUE_GPIO),
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    gpio_set_level(RGB_LED_RED_GPIO,   0);
    gpio_set_level(RGB_LED_GREEN_GPIO, 0);
    gpio_set_level(RGB_LED_BLUE_GPIO,  0);

    g_initialized = true;
}

void rgb_led_wifi_app_started(void)
{
    if (!g_initialized) rgb_led_init();
    gpio_set_level(RGB_LED_RED_GPIO,   1);
    gpio_set_level(RGB_LED_GREEN_GPIO, 0);
    gpio_set_level(RGB_LED_BLUE_GPIO,  1);
}

void rgb_led_http_server_started(void)
{
    if (!g_initialized) rgb_led_init();
    gpio_set_level(RGB_LED_RED_GPIO,   1);
    gpio_set_level(RGB_LED_GREEN_GPIO, 1);
    gpio_set_level(RGB_LED_BLUE_GPIO,  0);
}

void rgb_led_wifi_connected(void)
{
    if (!g_initialized) rgb_led_init();
    gpio_set_level(RGB_LED_RED_GPIO,   0);
    gpio_set_level(RGB_LED_GREEN_GPIO, 1);
    gpio_set_level(RGB_LED_BLUE_GPIO,  1);
}

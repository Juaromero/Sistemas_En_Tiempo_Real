/*
 * rgb_led.h
 *
 * LED RGB de estado WiFi.
 * Usa GPIO directo (on/off) para evitar conflictos de canales LEDC.
 * Pines: GPIO21=Rojo, GPIO22=Verde, GPIO23=Azul (catodo comun).
 */

#ifndef MAIN_RGB_LED_H_
#define MAIN_RGB_LED_H_

#define RGB_LED_RED_GPIO    21
#define RGB_LED_GREEN_GPIO  22
#define RGB_LED_BLUE_GPIO   23

void rgb_led_init(void);
void rgb_led_wifi_app_started(void);
void rgb_led_http_server_started(void);
void rgb_led_wifi_connected(void);

#endif /* MAIN_RGB_LED_H_ */

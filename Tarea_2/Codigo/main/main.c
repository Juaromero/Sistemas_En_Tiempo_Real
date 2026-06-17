/*
 * ============================================================
 *  MODO DE DETECCIÓN DEL BOTÓN: INTERRUPCIONES (ISR)
 * ============================================================
 */

// Librerías estándar de C
#include <stdio.h>        // printf y funciones de entrada/salida
#include <string.h>       // funciones de manejo de strings
#include <stdlib.h>       // funciones generales (malloc, free, etc.)
#include <inttypes.h>     // tipos enteros con tamaño fijo (uint32_t, etc.)

// Librerías de ESP-IDF
#include "freertos/FreeRTOS.h"  // núcleo de FreeRTOS
#include "freertos/task.h"      // manejo de tareas
#include "freertos/queue.h"     // manejo de colas
#include "driver/gpio.h"        // control de pines GPIO

// Pin del LED externo — cambia por el GPIO donde conectaste tu LED
#define LED_GPIO     GPIO_NUM_2

// Pin del botón BOOT de la placa ESP32-C6
#define BUTTON_GPIO  GPIO_NUM_9

// Tiempo mínimo entre interrupciones para evitar rebotes del botón (en ms)
#define DEBOUNCE_MS  200

// El typedef enum se declara PRIMERO para que todo el archivo lo conozca
typedef enum {
    LED_2_2,    // encendido 2s, apagado 2s
    LED_2_1,    // encendido 2s, apagado 1s
    LED_1_1,    // encendido 1s, apagado 1s
    LED_05_05,  // encendido 0.5s, apagado 0.5s
    LED_OFF     // LED apagado permanentemente
} led_enum_state;

// Handle de la cola que comunica la ISR con led_task
static QueueHandle_t led_queue = NULL;

// Ahora sí se puede usar led_enum_state porque ya fue declarado arriba
// Estado actual del LED, global para que la ISR pueda modificarlo
static led_enum_state led_state = LED_2_2;

// ============================================================
//  ISR — Interrupt Service Routine
//  Se ejecuta AUTOMÁTICAMENTE cada vez que el botón es presionado
//  IRAM_ATTR: obliga a que esta función esté en RAM para mayor velocidad
// ============================================================
static void IRAM_ATTR button_isr_handler(void *arg)
{
    // Variable estática para guardar el tiempo del último disparo
    // static significa que mantiene su valor entre llamadas
    static uint32_t last_isr_time = 0;

    // Obtiene el tiempo actual en milisegundos desde que arrancó el sistema
    uint32_t current_time = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

    // Anti-rebote: ignora la interrupción si ocurrió hace menos de DEBOUNCE_MS
    if ((current_time - last_isr_time) < DEBOUNCE_MS) {
        return; // sale de la ISR sin hacer nada
    }

    // Actualiza el tiempo del último disparo válido
    last_isr_time = current_time;

    // Avanza al siguiente estado en el ciclo
    switch (led_state)
    {
    case LED_2_2:   led_state = LED_2_1;   break;
    case LED_2_1:   led_state = LED_1_1;   break;
    case LED_1_1:   led_state = LED_05_05; break;
    case LED_05_05: led_state = LED_OFF;   break;
    case LED_OFF:   led_state = LED_2_2;   break;
    }

    // Envía el nuevo estado a la cola desde la ISR
    // Se usa xQueueSendFromISR en lugar de xQueueSend porque estamos en una ISR
    BaseType_t high_task_woken = pdFALSE;
    xQueueSendFromISR(led_queue, &led_state, &high_task_woken);

    // Si una tarea de mayor prioridad fue despertada, fuerza un cambio de contexto
    if (high_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

// Función que configura el hardware: LED como salida y botón con interrupción
void config_LED_and_BUTTON(void)
{
    gpio_config_t io_conf = {};

    // ── Configuración del LED ──────────────────────────────────────
    io_conf.intr_type    = GPIO_INTR_DISABLE;    // sin interrupción en el LED
    io_conf.mode         = GPIO_MODE_OUTPUT;      // el ESP32 envía señal al LED
    io_conf.pin_bit_mask = 1ULL << LED_GPIO;      // selecciona el pin del LED
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // no necesita pull-down
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;   // no necesita pull-up
    gpio_config(&io_conf);

    // ── Configuración del botón con interrupción ───────────────────
    // GPIO_INTR_NEGEDGE: dispara en flanco de bajada (cuando pin pasa de 1 a 0)
    io_conf.intr_type    = GPIO_INTR_NEGEDGE;
    io_conf.mode         = GPIO_MODE_INPUT;       // el ESP32 lee el estado del botón
    io_conf.pin_bit_mask = 1ULL << BUTTON_GPIO;   // selecciona el pin del botón
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;    // pull-up: por defecto lee 1
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // no se necesita pull-down
    gpio_config(&io_conf);

    // Instala el servicio de interrupciones GPIO del ESP32
    gpio_install_isr_service(0);

    // Asocia el pin del botón con la función ISR
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
}

// Tarea que controla el parpadeo del LED según el estado recibido
void led_task(void *pvParameters)
{
    // Estado local del LED
    led_enum_state led_state_2 = LED_2_2;

    // Bucle infinito: las tareas de FreeRTOS nunca deben terminar
    while (1){

        // Revisa si hay un nuevo estado en la cola sin bloquear (timeout 0)
        // Si no hay nada, continúa con el estado actual
        xQueueReceive(led_queue, &led_state_2, 0);

        if (led_state_2 == LED_2_2){
            gpio_set_level(LED_GPIO, 1);           // enciende el LED
            vTaskDelay(2000 / portTICK_PERIOD_MS); // espera 2 segundos
            gpio_set_level(LED_GPIO, 0);           // apaga el LED
            vTaskDelay(2000 / portTICK_PERIOD_MS); // espera 2 segundos
        }
        else if (led_state_2 == LED_2_1){
            gpio_set_level(LED_GPIO, 1);           // enciende el LED
            vTaskDelay(2000 / portTICK_PERIOD_MS); // espera 2 segundos
            gpio_set_level(LED_GPIO, 0);           // apaga el LED
            vTaskDelay(1000 / portTICK_PERIOD_MS); // espera 1 segundo
        }
        else if (led_state_2 == LED_1_1){
            gpio_set_level(LED_GPIO, 1);           // enciende el LED
            vTaskDelay(1000 / portTICK_PERIOD_MS); // espera 1 segundo
            gpio_set_level(LED_GPIO, 0);           // apaga el LED
            vTaskDelay(1000 / portTICK_PERIOD_MS); // espera 1 segundo
        }
        else if (led_state_2 == LED_05_05){
            gpio_set_level(LED_GPIO, 1);           // enciende el LED
            vTaskDelay(500 / portTICK_PERIOD_MS);  // espera 0.5 segundos
            gpio_set_level(LED_GPIO, 0);           // apaga el LED
            vTaskDelay(500 / portTICK_PERIOD_MS);  // espera 0.5 segundos
        }
        else if (led_state_2 == LED_OFF){
            gpio_set_level(LED_GPIO, 0);           // mantiene el LED apagado
            vTaskDelay(100 / portTICK_PERIOD_MS);  // pausa para no saturar CPU
        }
    }
}

// Punto de entrada del programa
void app_main(void)
{
    // Configura los pines del LED y registra la interrupción del botón
    config_LED_and_BUTTON();

    // Crea la cola de comunicación entre la ISR y led_task
    led_queue = xQueueCreate(1, sizeof(led_enum_state));

    // Envía el estado inicial para que el LED empiece a parpadear de inmediato
    xQueueSend(led_queue, &led_state, 0);

    // Crea la tarea del LED
    // Ya no hay button_task porque el botón se maneja por interrupción
    xTaskCreate(led_task, "led_task", 2048, NULL, 1, NULL);
}
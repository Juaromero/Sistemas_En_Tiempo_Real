/*
 * ============================================================
 *  MODO DE DETECCIÓN DEL BOTÓN: POLLING (SIN INTERRUPCIONES)
 * ============================================================
 */

// Librerías estándar de C
#include <stdio.h>        // printf y funciones de entrada/salida
#include <string.h>       // funciones de manejo de strings
#include <stdlib.h>       // funciones generales (malloc, free, etc.)
#include <inttypes.h>     // tipos enteros con tamaño fijo (uint32_t, etc.)

// Librerías de ESP-IDF (sistema operativo y hardware)
#include "freertos/FreeRTOS.h"  // núcleo de FreeRTOS (sistema operativo en tiempo real)
#include "freertos/task.h"      // manejo de tareas (xTaskCreate, vTaskDelay)
#include "freertos/queue.h"     // manejo de colas para comunicar tareas
#include "driver/gpio.h"        // control de pines GPIO del ESP32

// Definición del pin donde está conectado el LED externo
// Cambia GPIO_NUM_2 por el GPIO donde conectaste tu LED
#define LED_GPIO     GPIO_NUM_2

// Definición del pin del botón BOOT de la placa ESP32-C6
// En el ESP32-C6 el botón BOOT está conectado al GPIO9
#define BUTTON_GPIO  GPIO_NUM_9

// Handle de la cola que comunica button_task con led_task
// Se declara como static para que solo sea visible en este archivo
// Empieza en NULL porque se inicializa en app_main
static QueueHandle_t led_queue = NULL;

// Enumeración con los 5 estados posibles del LED
// Cada valor representa un patrón de parpadeo diferente
typedef enum {
    LED_2_2,    // encendido 2s, apagado 2s
    LED_2_1,    // encendido 2s, apagado 1s
    LED_1_1,    // encendido 1s, apagado 1s
    LED_05_05,  // encendido 0.5s, apagado 0.5s
    LED_OFF     // LED apagado permanentemente
} led_enum_state;

// Función que configura el hardware: LED como salida y botón como entrada
void config_LED_and_BUTTON(void)
{
    // Declara la estructura de configuración GPIO e inicializa todo en 0
    gpio_config_t io_conf = {};

    // ── Configuración del LED ──────────────────────────────────────
    // Sin interrupción: el LED no necesita interrupciones, se controla directo
    io_conf.intr_type = GPIO_INTR_DISABLE;

    // Modo salida: el ESP32 enviará señales al LED (no leerá)
    io_conf.mode = GPIO_MODE_OUTPUT;

    // Máscara de bits: indica qué pin configurar
    // "1ULL << LED_GPIO" pone un 1 en la posición del pin del LED
    io_conf.pin_bit_mask = 1ULL << LED_GPIO;

    // Sin pull-down: el LED es salida, no necesita resistencia a GND
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    // Sin pull-up: el LED es salida, no necesita resistencia a 3.3V
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    // Aplica la configuración al hardware del GPIO del LED
    gpio_config(&io_conf);

    // ── Configuración del botón ────────────────────────────────────
    // Modo entrada: el ESP32 leerá el estado del botón
    io_conf.mode = GPIO_MODE_INPUT;

    // Selecciona el pin del botón BOOT
    io_conf.pin_bit_mask = 1ULL << BUTTON_GPIO;

    // Activa pull-up: el botón BOOT va a GND al presionarse,
    // por eso el pin debe leer 1 por defecto
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    // Sin pull-down: ya usamos pull-up, no se necesitan ambos
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    // Aplica la configuración al hardware del GPIO del botón
    gpio_config(&io_conf);
}

// Tarea que monitorea el botón y envía el nuevo estado a la cola
// pvParameters: parámetro genérico de FreeRTOS (no se usa aquí)
void button_task(void *pvParameters)
{
    // Estado actual del LED, empieza en el primer modo de parpadeo
    led_enum_state led_state = LED_2_2;

    // Bucle infinito: las tareas de FreeRTOS nunca deben terminar
    while (1){

        // POLLING: pregunta cada 10ms si el botón está presionado
        // Con pull-up, 0 significa presionado
        if (gpio_get_level(BUTTON_GPIO) == 0){

            // Avanza al siguiente estado en el ciclo
            switch (led_state)
            {
            case LED_2_2:   led_state = LED_2_1;   break;
            case LED_2_1:   led_state = LED_1_1;   break;
            case LED_1_1:   led_state = LED_05_05; break;
            case LED_05_05: led_state = LED_OFF;   break;
            case LED_OFF:   led_state = LED_2_2;   break; // vuelve al inicio
            }

            // Envía el nuevo estado a la cola para que led_task lo reciba
            // Parámetros: (cola, dato a enviar, tiempo de espera si está llena)
            // 0 = no esperar si la cola está llena, descartar el envío
            xQueueSend(led_queue, &led_state, 0);

            // Anti-rebote: espera a que el botón sea soltado
            // El rebote ocurre porque mecánicamente el botón "vibra" al presionarse
            while (gpio_get_level(BUTTON_GPIO) == 0) {
                // Espera 100ms cediendo el CPU a otras tareas
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }

            // Espera adicional de 100ms después de soltar para estabilizar
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        // Pequeña pausa de 10ms cuando el botón no está presionado
        // Evita que esta tarea consuma el 100% del CPU en el bucle
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// Tarea que controla el parpadeo del LED según el estado recibido
// pvParameters: parámetro genérico de FreeRTOS (no se usa aquí)
void led_task(void *pvParameters)
{
    // Estado actual del LED, empieza en el primer modo de parpadeo
    led_enum_state led_state_2 = LED_2_2;

    // Bucle infinito: las tareas de FreeRTOS nunca deben terminar
    while (1){

        // Intenta recibir un nuevo estado de la cola
        // Parámetros: (cola, dónde guardar el dato, tiempo de espera)
        // 0 = no esperar, si no hay dato nuevo continúa con el estado actual
        // El & es para pasar la dirección de memoria, no una copia del valor
        xQueueReceive(led_queue, &led_state_2, 0);

        // Ejecuta el patrón de parpadeo según el estado actual
        if (led_state_2 == LED_2_2){
            gpio_set_level(LED_GPIO, 1);                    // enciende el LED
            vTaskDelay(2000 / portTICK_PERIOD_MS);          // espera 2 segundos
            gpio_set_level(LED_GPIO, 0);                    // apaga el LED
            vTaskDelay(2000 / portTICK_PERIOD_MS);          // espera 2 segundos
        }
        else if (led_state_2 == LED_2_1){
            gpio_set_level(LED_GPIO, 1);                    // enciende el LED
            vTaskDelay(2000 / portTICK_PERIOD_MS);          // espera 2 segundos
            gpio_set_level(LED_GPIO, 0);                    // apaga el LED
            vTaskDelay(1000 / portTICK_PERIOD_MS);          // espera 1 segundo
        }
        else if (led_state_2 == LED_1_1){
            gpio_set_level(LED_GPIO, 1);                    // enciende el LED
            vTaskDelay(1000 / portTICK_PERIOD_MS);          // espera 1 segundo
            gpio_set_level(LED_GPIO, 0);                    // apaga el LED
            vTaskDelay(1000 / portTICK_PERIOD_MS);          // espera 1 segundo
        }
        else if (led_state_2 == LED_05_05){
            gpio_set_level(LED_GPIO, 1);                    // enciende el LED
            vTaskDelay(500 / portTICK_PERIOD_MS);           // espera 0.5 segundos
            gpio_set_level(LED_GPIO, 0);                    // apaga el LED
            vTaskDelay(500 / portTICK_PERIOD_MS);           // espera 0.5 segundos
        }
        else if (led_state_2 == LED_OFF){
            gpio_set_level(LED_GPIO, 0);                    // mantiene el LED apagado
            vTaskDelay(100 / portTICK_PERIOD_MS);           // pausa para no saturar CPU
        }
    }
}

// Punto de entrada del programa (equivalente al main() en C estándar)
void app_main(void)
{
    // Configura los pines del LED y del botón
    config_LED_and_BUTTON();

    // Crea la cola de comunicación entre button_task y led_task
    // Parámetros: (cantidad máxima de elementos, tamaño de cada elemento en bytes)
    // Solo guarda 1 elemento: si llega uno nuevo antes de leerlo, se descarta el viejo
    led_queue = xQueueCreate(1, sizeof(led_enum_state));

    // Envía el estado inicial para que el LED empiece a parpadear inmediatamente
    led_enum_state initial_state = LED_2_2;
    xQueueSend(led_queue, &initial_state, 0);

    // Crea la tarea del botón
    // Parámetros: (función, nombre, stack en bytes, parámetro, prioridad, handle)
    xTaskCreate(button_task, "button_task", 2048, NULL, 1, NULL);

    // Crea la tarea del LED
    // Parámetros: (función, nombre, stack en bytes, parámetro, prioridad, handle)
    xTaskCreate(led_task, "led_task", 2048, NULL, 1, NULL);
}
#ifndef NTC_H
#define NTC_H

#include "config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

// =====================================================
// FUNCIONES PUBLICAS
// =====================================================

// Inicializa el ADC (NTC en CH0, Potenciometro en CH1)
void adc_init(void);

// Lee un canal ADC y retorna el valor en milivolts
int leer_adc_mv(adc_channel_t canal);

// Calcula la temperatura en grados Celsius a partir de la NTC
float leer_temperatura(void);

// Convierte temperatura desde Celsius a la unidad indicada
float convertir_temperatura(float temp_c, int unidad);

// Retorna el simbolo de la unidad ("C", "F", "K")
const char *simbolo_unidad(int unidad);

// Tarea FreeRTOS: lee NTC segun el intervalo configurado
// y actualiza cfg->temperatura
void temperature_task(void *pvParameters);

#endif
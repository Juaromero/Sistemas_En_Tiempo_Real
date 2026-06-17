#ifndef RGB_H
#define RGB_H

#include "config.h"
#include "driver/ledc.h"

// =====================================================
// PWM
// =====================================================

#define PWM_FREQ_HZ     5000
#define PWM_RESOLUTION  LEDC_TIMER_13_BIT
#define PWM_MAX_DUTY    8191    // 2^13 - 1

// Canales LEDC asignados al LED RGB (catodo comun)
#define CH_R1   LEDC_CHANNEL_0
#define CH_G1   LEDC_CHANNEL_1
#define CH_B1   LEDC_CHANNEL_2

// =====================================================
// FUNCIONES PUBLICAS
// =====================================================

void rgb_init(void);
void set_rgb(float r, float g, float b);

// Tarea FreeRTOS: actualiza el LED cada 100ms segun el modo activo
//
// MODO 2 — TEMP UART:
//   Enciende canal al 100% si la temperatura (Celsius internamente)
//   cae dentro del rango lim_red/green/blue
//
// MODO 3 — INT UART:
//   Intensidad fija por canal segun comandos SET INT
//
// MODO 4 — POT UMBRAL:
//   0-33%  -> ROJO  (100%)
//   34-66% -> VERDE (100%)
//   67-100%-> AZUL  (100%)
void rgb_task(void *pvParameters);

#endif
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

// Inicializa el timer PWM y los 3 canales del LED RGB
void rgb_init(void);

// Establece el color del LED (r, g, b en rango 0.0 - 1.0)
// Catodo comun: duty alto = mas brillo
void set_rgb(float r, float g, float b);

// Tarea FreeRTOS: actualiza el LED cada 100ms segun el modo activo
void rgb_task(void *pvParameters);

#endif
#ifndef BUTTONS_H
#define BUTTONS_H

#include "config.h"
#include "driver/gpio.h"

// =====================================================
// FUNCIONES PUBLICAS
// =====================================================

// Configura GPIO de los botones con pull-up interno
void buttons_init(void);

// Tarea FreeRTOS:
//   BTN_MODO (GPIO4) -> cicla modos 2 -> 3 -> 4 -> 2
//   BTN_BOOT (GPIO9) -> cicla unidad C -> F -> K -> C
void buttons_task(void *pvParameters);

#endif
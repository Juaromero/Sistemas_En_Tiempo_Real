#ifndef BUTTONS_H
#define BUTTONS_H
 
#include "config.h"
#include "driver/gpio.h"
 
// =====================================================
// FUNCIONES PUBLICAS
// =====================================================
 
// Configura GPIO de los botones con pull-up interno
void buttons_init(void);
 
// Tarea FreeRTOS: maneja cambio de modo y configuracion
// del potenciometro mediante BTN_MODO y BTN_GUARDAR
void buttons_task(void *pvParameters);
 
#endif
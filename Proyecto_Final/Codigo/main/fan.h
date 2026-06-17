#ifndef FAN_H
#define FAN_H

#include "config.h"

// =====================================================
// FUNCIONES PUBLICAS
// =====================================================

// Inicializa LEDC para el fan y GPIO para LED alarma
void fan_init(void);

// Establece velocidad del fan 0-100% via LEDC
void fan_set_speed(int porcentaje);

// Tarea FreeRTOS:
//   - Modo automatico: control proporcional segun temperatura
//   - Modo manual: aplica fan_manual_pct directamente
//   - Alarma: parpadeo LED_ALARM_GPIO a 1 Hz si temp > fan_temp_maxima
void fan_task(void *pvParameters);

#endif
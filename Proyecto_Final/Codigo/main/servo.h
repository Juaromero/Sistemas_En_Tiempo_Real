#ifndef SERVO_H
#define SERVO_H

#include "config.h"

// =====================================================
// FUNCIONES PUBLICAS
// =====================================================

// Inicializa LEDC para el servo (50 Hz, timer 2)
void servo_init(void);

// Mueve el servo a un porcentaje de apertura 0-100%
// Internamente convierte a pulso us entre SERVO_PULSE_MIN_US y SERVO_PULSE_MAX_US
void servo_set_position(int porcentaje);

// Tarea FreeRTOS:
//   - Modo manual: aplica servo_manual_pct directamente
//   - Modo automatico: revisa servo_horarios y ejecuta
//     el horario cuya hora:minuto coincida con la hora actual
void servo_task(void *pvParameters);

#endif
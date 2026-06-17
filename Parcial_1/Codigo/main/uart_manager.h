#ifndef UART_MANAGER_H
#define UART_MANAGER_H

#include "config.h"
#include "driver/uart.h"

// =====================================================
// UART CONFIG
// =====================================================

#define UART_PORT_NUM       UART_NUM_0
#define UART_BAUD_RATE      115200
#define UART_BUFFER_SIZE    1024

// =====================================================
// PROTOCOLO DE COMANDOS
// =====================================================
//
// Disponibles en cualquier modo:
//   GET TEMP                              temperatura actual (unidad activa)
//   SET UNIT <C|F|K>                      cambia unidad de temperatura
//   SET INTERVAL <1-60>                   cambia intervalo de impresion (segundos)
//
// Modo 2 — rangos de temperatura configurables:
//   SET LIM <RED|GREEN|BLUE> <min> <max>  valores en la unidad activa
//
// Modo 3 — intensidad manual:
//   SET INT <RED|GREEN|BLUE> <0-100>
//
// Respuestas:
//   OK <comando>
//   TEMP <valor> <unidad>
//   ERROR <motivo>
//
// =====================================================

void uart_init(void);
void uart_task(void *pvParameters);

#endif
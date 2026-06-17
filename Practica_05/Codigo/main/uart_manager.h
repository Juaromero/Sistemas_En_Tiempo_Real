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
//   GET TEMP                           retorna temperatura actual
//   GET COLOR                          retorna color guardado (Modo 4)
//
// Modo 2 — rangos de temperatura configurables:
//   SET LIM RED   <min> <max>          grados Celsius
//   SET LIM GREEN <min> <max>
//   SET LIM BLUE  <min> <max>
//
// Modo 3 — intensidad manual:
//   SET INT RED   <0-100>
//   SET INT GREEN <0-100>
//   SET INT BLUE  <0-100>
//
// Respuestas:
//   OK <comando>
//   TEMP <valor> C
//   POT R:<val> G:<val> B:<val>        (en porcentaje)
//   ERROR <motivo>
//
// =====================================================

void uart_init(void);
void uart_task(void *pvParameters);

#endif
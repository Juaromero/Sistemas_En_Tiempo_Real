#ifndef CONFIG_H
#define CONFIG_H
 
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
 
// =====================================================
// PINES ADC
// =====================================================
 
#define ADC_NTC_CHANNEL     ADC_CHANNEL_0   // GPIO0 — NTC
#define ADC_POT_CHANNEL     ADC_CHANNEL_1   // GPIO1 — Potenciometro
#define ADC_UNIT            ADC_UNIT_1
#define ADC_SAMPLES         10
 
// =====================================================
// BOTONES
// =====================================================
 
#define BTN_MODO            GPIO_NUM_4      // Cicla modos 2->3->4->2
#define BTN_BOOT            GPIO_NUM_9      // Cicla unidad C->F->K->C
 
// =====================================================
// RGB — LED catodo comun
// =====================================================
 
#define R1  GPIO_NUM_6
#define G1  GPIO_NUM_7
#define B1  GPIO_NUM_8
 
// =====================================================
// NTC — 10K NTC (Beta 4100K)
// Circuito: 3.3V -> NTC -> ADC -> R_serie(10K) -> GND
// =====================================================
 
#define NTC_SERIES_R        10000.0f
#define NTC_NOMINAL_R       10000.0f
#define NTC_NOMINAL_T       25.0f
#define NTC_BETA            4100.0f
 
// =====================================================
// MODOS DEL SISTEMA
// =====================================================
 
#define MODO_TEMP_UART      2   // NTC con rangos configurables por UART
#define MODO_INT_UART       3   // Intensidad manual por UART
#define MODO_POT_UMBRAL     4   // Potenciometro con umbrales de color
#define MODO_MIN            2
#define MODO_MAX            4
 
// =====================================================
// UNIDADES DE TEMPERATURA
// =====================================================
 
#define UNIDAD_CELSIUS      0
#define UNIDAD_FAHRENHEIT   1
#define UNIDAD_KELVIN       2
 
// =====================================================
// INTERVALO DE IMPRESION DE TEMPERATURA
// =====================================================
 
#define TEMP_INTERVAL_MIN   1       // segundos
#define TEMP_INTERVAL_MAX   60      // segundos
 
// =====================================================
// UMBRALES DEL POTENCIOMETRO — MODO 4
//   0  .. UMBRAL_ROJO  (0-33%)  -> ROJO
//   34 .. UMBRAL_VERDE (34-66%) -> VERDE
//   67 .. 100          (67-100%)-> AZUL
// =====================================================
 
#define UMBRAL_ROJO         33
#define UMBRAL_VERDE        66
 
// =====================================================
// ESTRUCTURAS
// =====================================================
 
typedef struct
{
    float min;
    float max;
} rango_t;
 
typedef struct
{
    // --- Modo 2: rangos de temperatura configurables por UART (en Celsius) ---
    rango_t lim_red;
    rango_t lim_green;
    rango_t lim_blue;
 
    // --- Modo 3: intensidad manual por UART (0.0 - 1.0) ---
    float int_red;
    float int_green;
    float int_blue;
 
    // --- Modo 4: valor actual del potenciometro ---
    float pot_valor;    // 0.0 - 1.0
    int   pot_pct;      // 0 - 100
 
    // --- Temperatura actual (siempre almacenada en Celsius) ---
    float temperatura;
 
    // --- Unidad de temperatura activa ---
    int unidad;         // UNIDAD_CELSIUS / FAHRENHEIT / KELVIN
 
    // --- Intervalo de impresion de temperatura (ms) ---
    int intervalo_ms;
 
    // --- Modo activo (2, 3 o 4) ---
    int modo;
 
    // --- Mutex compartido ---
    SemaphoreHandle_t mutex;
 
} system_config_t;
 
#endif
 
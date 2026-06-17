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

#define BTN_MODO            GPIO_NUM_4
#define BTN_GUARDAR         GPIO_NUM_5

// =====================================================
// RGB — LED catodo comun
// =====================================================

#define R1  GPIO_NUM_6
#define G1  GPIO_NUM_7
#define B1  GPIO_NUM_8

// =====================================================
// NTC — 10K NTC (Beta 4100K)
// Circuito: 3.3V -> NTC -> ADC -> R_serie(10K) -> GND
// El ADC mide el voltaje sobre R_serie (no sobre NTC).
// R_NTC = R_serie * (Vcc - Vadc) / Vadc
// =====================================================

#define NTC_SERIES_R        10000.0f    // Resistencia de carga en GND
#define NTC_NOMINAL_R       10000.0f    // Resistencia NTC a 25°C
#define NTC_NOMINAL_T       25.0f       // Temperatura nominal (°C)
#define NTC_BETA            4100.0f     // Coeficiente Beta (K)

// =====================================================
// RANGOS FIJOS DE TEMPERATURA — MODO 1
// Definidos aqui, no modificables en tiempo real
//   < TEMP_FRIO          -> AZUL
//   TEMP_FRIO .. TEMP_CALIENTE -> VERDE
//   > TEMP_CALIENTE      -> ROJO
// =====================================================

#define TEMP_FRIO           25.0f   // por debajo: frio (azul)
#define TEMP_CALIENTE       35.0f   // por encima: caliente (rojo)
                                    // en medio:   normal  (verde)

// =====================================================
// MODOS DEL SISTEMA (numerados desde 1)
// =====================================================

#define MODO_TEMP_FIJO      1   // NTC con rangos fijos (hardcoded)
#define MODO_TEMP_UART      2   // NTC con rangos configurables por UART
#define MODO_INT_UART       3   // Intensidad manual por UART
#define MODO_POTENCIOMETRO  4   // Color configurado con potenciometro
#define MODO_MIN            1
#define MODO_MAX            4

// =====================================================
// ESTADO INTERNO DEL POTENCIOMETRO (Modo 4)
// =====================================================

#define POT_CONFIG_RED      0   // Configurando canal rojo
#define POT_CONFIG_BLUE     1   // Configurando canal azul
#define POT_CONFIG_GREEN    2   // Configurando canal verde
#define POT_LISTO           3   // Los 3 canales ya fueron guardados

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
    // --- Modo 2: rangos de temperatura configurables por UART ---
    rango_t lim_red;
    rango_t lim_green;
    rango_t lim_blue;

    // --- Modo 3: intensidad manual por UART (0.0 - 1.0) ---
    float int_red;
    float int_green;
    float int_blue;

    // --- Modo 4: color configurado con potenciometro ---
    float pot_red;
    float pot_green;
    float pot_blue;
    int   pot_estado;   // POT_CONFIG_RED / BLUE / GREEN / LISTO
    int   pot_listos;   // 1 cuando los 3 canales fueron guardados
    float pot_valor;    // lectura actual del pot (0.0 - 1.0)

    // --- Temperatura actual (grados Celsius) ---
    float temperatura;

    // --- Modo activo (1 a 4) ---
    int modo;

    // --- Mutex compartido ---
    SemaphoreHandle_t mutex;

} system_config_t;

#endif
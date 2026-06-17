
#ifndef CONFIG_H
#define CONFIG_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// =====================================================
// PINES ADC
// =====================================================

#define ADC_NTC_CHANNEL     ADC_CHANNEL_0   // GPIO0 — NTC
#define ADC_POT_CHANNEL     ADC_CHANNEL_1   // GPIO1 — Potenciometro (proyecto anterior)
#define ADC_UNIT            ADC_UNIT_1
#define ADC_SAMPLES         10

// =====================================================
// BOTONES
// =====================================================

#define BTN_MODO            GPIO_NUM_4      // Cicla modos (proyecto anterior)
#define BTN_BOOT            GPIO_NUM_9      // Cicla unidad C->F->K->C

// =====================================================
// RGB — LED catodo comun
// =====================================================

#define R1  GPIO_NUM_6
#define G1  GPIO_NUM_7
#define B1  GPIO_NUM_8

// =====================================================
// FAN — Ventilador 12V via MOSFET
// GPIO15 -> gate MOSFET (IRLZ44N) -> ventilador
// =====================================================

#define FAN_GPIO            GPIO_NUM_15
#define FAN_LEDC_CHANNEL    LEDC_CHANNEL_3
#define FAN_LEDC_TIMER      LEDC_TIMER_1
#define FAN_PWM_FREQ_HZ     25000           // 25 kHz — inaudible para el motor
#define FAN_PWM_RESOLUTION  LEDC_TIMER_10_BIT
#define FAN_PWM_MAX_DUTY    1023            // 2^10 - 1

// =====================================================
// LED ALARMA — LED rojo parpadeo 1 Hz
// =====================================================

#define LED_ALARM_GPIO      GPIO_NUM_2      // GPIO2 — LED rojo alarma

// =====================================================
// SERVO MOTOR — Cortinas
// Senal PWM: 50Hz, pulso 500us(0%) a 2500us(100%)
// =====================================================

#define SERVO_GPIO          GPIO_NUM_20
#define SERVO_LEDC_CHANNEL  LEDC_CHANNEL_4
#define SERVO_LEDC_TIMER    LEDC_TIMER_2
#define SERVO_PWM_FREQ_HZ   50              // 50 Hz estandar servo
#define SERVO_PWM_RESOLUTION LEDC_TIMER_14_BIT
#define SERVO_PWM_MAX_DUTY  16383           // 2^14 - 1
#define SERVO_PULSE_MIN_US  500             // pulso minimo = 0%
#define SERVO_PULSE_MAX_US  2500            // pulso maximo = 100%
#define SERVO_MAX_SCHEDULES 8               // registros de horario

// =====================================================
// NTC — 10K NTC (Beta 4100K)
// Circuito: 3.3V -> NTC -> ADC -> R_serie(10K) -> GND
// =====================================================

#define NTC_SERIES_R        10000.0f
#define NTC_NOMINAL_R       10000.0f
#define NTC_NOMINAL_T       25.0f
#define NTC_BETA            4100.0f

// =====================================================
// WI-FI
// =====================================================

#define WIFI_STA_SSID_DEFAULT   "MiRed"
#define WIFI_STA_PASS_DEFAULT   "MiPassword"
#define WIFI_AP_SSID_DEFAULT    "STR_2026"
#define WIFI_AP_PASS_DEFAULT    "str2026pass"
#define WIFI_AP_MAX_CONN        4
#define WIFI_SSID_MAX_LEN       32
#define WIFI_PASS_MAX_LEN       64

// =====================================================
// MODOS DEL SISTEMA (compatibles con proyecto anterior)
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

// Un registro de horario para el servo
typedef struct
{
    int   hora;         // 0-23
    int   minuto;       // 0-59
    int   porcentaje;   // 0-100 (apertura de cortina)
    int   activo;       // 1 = habilitado, 0 = ignorado
} servo_schedule_t;

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

    // =========================================================
    // STR 2026 — nuevos campos
    // =========================================================

    // --- Fan: modo (0=automatico, 1=manual) ---
    int   fan_modo;             // 0 = auto temperatura, 1 = manual
    float fan_temp_deseada;     // temperatura objetivo (Celsius)
    float fan_temp_maxima;      // temperatura maxima (Celsius)
    int   fan_manual_pct;       // velocidad manual 0-100%
    int   fan_duty_actual;      // duty LEDC calculado (solo lectura)
    int   fan_alarma;           // 1 si temp > fan_temp_maxima

    // --- Servo / Cortinas ---
    int   servo_modo;           // 0 = automatico (horario), 1 = manual
    int   servo_manual_pct;     // apertura manual 0-100%
    int   servo_posicion_actual;// posicion actual 0-100%
    servo_schedule_t servo_horarios[SERVO_MAX_SCHEDULES];

    // --- RGB: brillo global (0.0 - 1.0) multiplicado sobre los canales ---
    float rgb_brillo;

    // --- Wi-Fi: credenciales STA (para conectar a router) ---
    char  wifi_sta_ssid[WIFI_SSID_MAX_LEN];
    char  wifi_sta_pass[WIFI_PASS_MAX_LEN];

    // --- Wi-Fi: credenciales AP (softAP propio del ESP32) ---
    char  wifi_ap_ssid[WIFI_SSID_MAX_LEN];
    char  wifi_ap_pass[WIFI_PASS_MAX_LEN];

    // --- Mutex compartido ---
    SemaphoreHandle_t mutex;

} system_config_t;

#endif

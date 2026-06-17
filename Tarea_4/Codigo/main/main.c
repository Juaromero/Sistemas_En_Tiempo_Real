/*
 * ============================================================
 *   ESP32-C6 — Controlador RGB con Sensor NTC  [ESP-IDF v5.x]
 * ============================================================
 *
 *  PROGRAMA 1 — Temperatura:
 *    · LED1 → Azul (<25°C) · Verde (25–35°C) · Rojo (>35°C)
 *    · LED2 → Muestra el color combinado guardado del Programa 2
 *
 *  PROGRAMA 2 — Configuración de Colores (4 estados):
 *    · Estado 1: Potenciometro define color ROJO   → BTN_SELECT guarda
 *    · Estado 2: Potenciometro define color AZUL   → BTN_SELECT guarda
 *    · Estado 3: Potenciometro define color VERDE  → BTN_SELECT guarda
 *    · Estado 4: LED2 muestra la mezcla de los 3 colores guardados
 *
 *  BTN_MODE (GPIO5): Alterna entre Programa 1 y Programa 2
 *
 *  Los colores se persisten en la memoria NVS (flash) del ESP32-C6.
 *  El LED1 en el Programa 2 actúa como indicador de estado.
 *
 * ============================================================
 *  CONEXIONES
 * ============================================================
 *  GPIO2  → NTC  (3.3V → NTC → GPIO2 → R10kΩ → GND)
 *  GPIO3  → Potenciometro (extremo 1: 3.3V · wiper: GPIO3 · extremo 2: GND)
 *  GPIO4  → Botón SELECT  (GPIO4 → botón → GND, pull-up interno)
 *  GPIO5  → Botón MODE    (GPIO5 → botón → GND, pull-up interno)
 *  GPIO6  → LED1 Rojo   (R 220Ω en serie)
 *  GPIO7  → LED1 Verde  (R 220Ω en serie)
 *  GPIO8  → LED1 Azul   (R 220Ω en serie)
 *  GPIO10 → LED2 Rojo   (R 220Ω en serie)
 *  GPIO11 → LED2 Verde  (R 220Ω en serie)
 *  GPIO12 → LED2 Azul   (R 220Ω en serie)
 *  Cátodo común de ambos LEDs → GND
 * ============================================================
 */

#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

/* ─────────────────────────────────────────────
   PINES
   ───────────────────────────────────────────── */
#define PIN_NTC           GPIO_NUM_2
#define PIN_POT           GPIO_NUM_3
#define PIN_BTN_SELECT    GPIO_NUM_4
#define PIN_BTN_MODE      GPIO_NUM_5

#define PIN_LED1_R        GPIO_NUM_6
#define PIN_LED1_G        GPIO_NUM_7
#define PIN_LED1_B        GPIO_NUM_8

#define PIN_LED2_R        GPIO_NUM_10
#define PIN_LED2_G        GPIO_NUM_11
#define PIN_LED2_B        GPIO_NUM_12

/* Canales ADC1 (GPIO2 = CH2, GPIO3 = CH3) */
#define ADC_CH_NTC        ADC_CHANNEL_2
#define ADC_CH_POT        ADC_CHANNEL_3

/* ─────────────────────────────────────────────
   PARÁMETROS NTC (Steinhart-Hart simplificado)
   ───────────────────────────────────────────── */
#define NTC_R_NOMINAL     10000.0f   /* Resistencia nominal a 25°C (10 kΩ) */
#define NTC_BETA          3950.0f    /* Coeficiente B del termistor        */
#define R_SERIE           10000.0f   /* Resistencia en serie (10 kΩ)       */
#define TEMP_NOMINAL_C    25.0f      /* Temperatura nominal (°C)           */
#define ADC_FULL_SCALE    4095

/* ─────────────────────────────────────────────
   CONFIGURACIÓN LEDC (PWM)
   ───────────────────────────────────────────── */
#define LEDC_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_TIMER_ID     LEDC_TIMER_0
#define LEDC_FREQ_HZ      5000
#define LEDC_RESOLUTION   LEDC_TIMER_8_BIT    /* 8 bits → duty 0–255 */

/* Canales para LED 1 */
#define CH_L1_R           LEDC_CHANNEL_0
#define CH_L1_G           LEDC_CHANNEL_1
#define CH_L1_B           LEDC_CHANNEL_2
/* Canales para LED 2 */
#define CH_L2_R           LEDC_CHANNEL_3
#define CH_L2_G           LEDC_CHANNEL_4
#define CH_L2_B           LEDC_CHANNEL_5

/* ─────────────────────────────────────────────
   DEBOUNCE Y TEMPORIZACIÓN
   ───────────────────────────────────────────── */
#define DEBOUNCE_MS       50
#define LOG_INTERVAL_MS   1000
#define LOOP_PERIOD_MS    20

/* ─────────────────────────────────────────────
   TAG DE LOGGING
   ───────────────────────────────────────────── */
static const char *TAG = "RGB_CTRL";

/* ─────────────────────────────────────────────
   TIPOS
   ───────────────────────────────────────────── */
typedef enum {
    PROG_TEMPERATURA = 0,
    PROG_COLOR       = 1,
} programa_t;

typedef enum {
    EST_ROJO      = 0,
    EST_AZUL      = 1,
    EST_VERDE     = 2,
    EST_COMBINADO = 3,
} estado_color_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

/* ─────────────────────────────────────────────
   VARIABLES GLOBALES
   ───────────────────────────────────────────── */
static programa_t     programa_actual = PROG_TEMPERATURA;
static estado_color_t estado_actual   = EST_ROJO;

static rgb_t color_rojo  = { .r = 255, .g =   0, .b =   0 };
static rgb_t color_azul  = { .r =   0, .g =   0, .b = 255 };
static rgb_t color_verde = { .r =   0, .g = 255, .b =   0 };

static adc_oneshot_unit_handle_t s_adc_handle = NULL;

/* ─────────────────────────────────────────────
   PROTOTIPOS
   ───────────────────────────────────────────── */
/* Inicialización */
static void nvs_init(void);
static void gpio_init(void);
static void ledc_init(void);
static void adc_init(void);

/* Lógica de programas */
static void ejecutar_programa1(void);
static void ejecutar_programa2(void);

/* Control de LEDs */
static void set_led1(uint8_t r, uint8_t g, uint8_t b);
static void set_led2(uint8_t r, uint8_t g, uint8_t b);
static void parpadear_led1(uint8_t r, uint8_t g, uint8_t b, int veces);

/* Sensores */
static float leer_temperatura(void);
static int   leer_pot(void);

/* Utilidades de color */
static rgb_t hsv_a_rgb(float h, float s, float v);
static rgb_t calcular_combinado(void);

/* Persistencia */
static void guardar_colores(void);
static void cargar_colores(void);

/* Botones con debounce */
static bool btn_select_presionado(void);
static bool btn_mode_presionado(void);

/* Auxiliar PWM interno */
static void pwm_write(ledc_channel_t canal, uint32_t duty);

/* ═════════════════════════════════════════════
   app_main
   ═════════════════════════════════════════════ */
void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-C6 RGB Controller — Iniciando ===");

    nvs_init();
    gpio_init();
    ledc_init();
    adc_init();
    cargar_colores();

    parpadear_led1(255, 255, 255, 2);   /* Parpadeo de confirmación de inicio */

    ESP_LOGI(TAG, "Programa activo: PROGRAMA 1 (Temperatura)");
    ESP_LOGI(TAG, "Presiona BTN_MODE (GPIO5) para cambiar programa");

    /* ── Bucle principal ─────────────────────── */
    while (1) {

        if (btn_mode_presionado()) {
            if (programa_actual == PROG_TEMPERATURA) {
                programa_actual = PROG_COLOR;
                estado_actual   = EST_ROJO;
                ESP_LOGI(TAG, ">>> PROGRAMA 2: Configuracion de Colores <<<");
                ESP_LOGI(TAG, "[Estado 1] Ajusta potenciometro y presiona BTN_SELECT");
            } else {
                programa_actual = PROG_TEMPERATURA;
                ESP_LOGI(TAG, ">>> PROGRAMA 1: Monitor de Temperatura <<<");
            }
            vTaskDelay(pdMS_TO_TICKS(200));   /* Pausa anti-rebote extra */
        }

        if (programa_actual == PROG_TEMPERATURA) {
            ejecutar_programa1();
        } else {
            ejecutar_programa2();
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}

/* ═════════════════════════════════════════════
   INICIALIZACIÓN
   ═════════════════════════════════════════════ */

static void nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS: borrando particion y reiniciando...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static void gpio_init(void)
{
    /* Botones SELECT y MODE como entradas digitales con pull-up interno.
       Se activan en nivel LOW al presionar. */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_BTN_SELECT) | (1ULL << PIN_BTN_MODE),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

static void ledc_init(void)
{
    /* ── Timer compartido para ambos LEDs ─────── */
    const ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER_ID,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz         = LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    /* ── Tabla de pines → canales ─────────────── */
    const struct { gpio_num_t pin; ledc_channel_t canal; } mapa[6] = {
        { PIN_LED1_R, CH_L1_R },
        { PIN_LED1_G, CH_L1_G },
        { PIN_LED1_B, CH_L1_B },
        { PIN_LED2_R, CH_L2_R },
        { PIN_LED2_G, CH_L2_G },
        { PIN_LED2_B, CH_L2_B },
    };

    for (int i = 0; i < 6; i++) {
        const ledc_channel_config_t ch_cfg = {
            .speed_mode = LEDC_MODE,
            .channel    = mapa[i].canal,
            .timer_sel  = LEDC_TIMER_ID,
            .intr_type  = LEDC_INTR_DISABLE,
            .gpio_num   = mapa[i].pin,
            .duty       = 0,
            .hpoint     = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
    }
}

static void adc_init(void)
{
    /* ── Unidad ADC1 ───────────────────────────── */
    const adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle));

    /* ── Canales NTC (GPIO2) y POT (GPIO3) ──────
       ADC_ATTEN_DB_12: rango de entrada 0 – 3.3V (ESP-IDF v5.2+)
       Usar ADC_ATTEN_DB_11 si se compila con una versión anterior. */
    const adc_oneshot_chan_cfg_t ch_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, ADC_CH_NTC, &ch_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, ADC_CH_POT, &ch_cfg));
}

/* ═════════════════════════════════════════════
   PROGRAMA 1 — Monitor de Temperatura
   ═════════════════════════════════════════════ */

static void ejecutar_programa1(void)
{
    float temp = leer_temperatura();

    /* LED1: color según rango de temperatura */
    if (temp < 25.0f) {
        set_led1(0, 0, 255);        /* Azul   — frío      */
    } else if (temp <= 35.0f) {
        set_led1(0, 255, 0);        /* Verde  — confort   */
    } else {
        set_led1(255, 0, 0);        /* Rojo   — caliente  */
    }

    /* LED2: muestra el color combinado del Programa 2 */
    rgb_t comb = calcular_combinado();
    set_led2(comb.r, comb.g, comb.b);

    /* Log periódico cada 1 segundo */
    static int64_t t_log = 0;
    int64_t ahora = esp_timer_get_time() / 1000LL;   /* µs → ms */
    if (ahora - t_log >= LOG_INTERVAL_MS) {
        t_log = ahora;
        const char *zona = (temp < 25.0f)   ? "FRIO"
                         : (temp <= 35.0f)  ? "CONFORT"
                                            : "CALIENTE";
        ESP_LOGI(TAG, "[PROG1] Temp: %.1f C (%s) | LED2 -> R:%d G:%d B:%d",
                 temp, zona, comb.r, comb.g, comb.b);
    }
}

/* ═════════════════════════════════════════════
   PROGRAMA 2 — Configuración de Colores
   ═════════════════════════════════════════════ */

static void ejecutar_programa2(void)
{
    /* Leer potenciometro y convertir a tono HSV (0–360°) */
    int   pot_raw = leer_pot();
    float matiz   = (pot_raw / (float)ADC_FULL_SCALE) * 360.0f;
    rgb_t preview = hsv_a_rgb(matiz, 1.0f, 1.0f);

    /* LED2 siempre muestra el color de previsualización */
    set_led2(preview.r, preview.g, preview.b);

    switch (estado_actual) {

        /* ── Estado 1: Definir color ROJO ──────── */
        case EST_ROJO:
            set_led1(80, 0, 0);   /* LED1 rojo tenue = indicador de estado */
            if (btn_select_presionado()) {
                color_rojo = preview;
                guardar_colores();
                parpadear_led1(255, 255, 255, 3);   /* Confirmación visual */
                estado_actual = EST_AZUL;
                ESP_LOGI(TAG, "[PROG2] ROJO guardado -> R:%d G:%d B:%d",
                         color_rojo.r, color_rojo.g, color_rojo.b);
                ESP_LOGI(TAG, "[Estado 2] Ajusta y presiona BTN_SELECT para guardar el AZUL");
            }
            break;

        /* ── Estado 2: Definir color AZUL ──────── */
        case EST_AZUL:
            set_led1(0, 0, 80);   /* LED1 azul tenue */
            if (btn_select_presionado()) {
                color_azul = preview;
                guardar_colores();
                parpadear_led1(255, 255, 255, 3);
                estado_actual = EST_VERDE;
                ESP_LOGI(TAG, "[PROG2] AZUL guardado -> R:%d G:%d B:%d",
                         color_azul.r, color_azul.g, color_azul.b);
                ESP_LOGI(TAG, "[Estado 3] Ajusta y presiona BTN_SELECT para guardar el VERDE");
            }
            break;

        /* ── Estado 3: Definir color VERDE ─────── */
        case EST_VERDE:
            set_led1(0, 80, 0);   /* LED1 verde tenue */
            if (btn_select_presionado()) {
                color_verde = preview;
                guardar_colores();
                parpadear_led1(255, 255, 255, 3);
                estado_actual = EST_COMBINADO;
                ESP_LOGI(TAG, "[PROG2] VERDE guardado -> R:%d G:%d B:%d",
                         color_verde.r, color_verde.g, color_verde.b);
                ESP_LOGI(TAG, "[Estado 4] COMBINADO — Mostrando mezcla de los 3 colores");
            }
            break;

        /* ── Estado 4: Color Combinado ──────────── */
        case EST_COMBINADO: {
            rgb_t comb = calcular_combinado();
            set_led1(255, 255, 255);      /* LED1 blanco = estado final     */
            set_led2(comb.r, comb.g, comb.b);   /* Sobreescribe el preview  */

            static int64_t t_log = 0;
            int64_t ahora = esp_timer_get_time() / 1000LL;
            if (ahora - t_log >= 2000LL) {
                t_log = ahora;
                ESP_LOGI(TAG, "[PROG2] COMBINADO -> R:%d G:%d B:%d",
                         comb.r, comb.g, comb.b);
                ESP_LOGI(TAG, "        BTN_SELECT reinicia la configuracion");
            }

            /* BTN_SELECT en Estado 4 reinicia el ciclo */
            if (btn_select_presionado()) {
                estado_actual = EST_ROJO;
                ESP_LOGI(TAG, "[PROG2] Ciclo reiniciado.");
                ESP_LOGI(TAG, "[Estado 1] Ajusta y presiona BTN_SELECT para guardar el ROJO");
            }
            break;
        }
    } /* switch */
}

/* ═════════════════════════════════════════════
   FUNCIONES DE HARDWARE
   ═════════════════════════════════════════════ */

/**
 * @brief Devuelve la temperatura en °C usando la ecuación de Steinhart-Hart.
 *        Circuito: 3.3V → NTC → GPIO2 → R_serie → GND
 */
static float leer_temperatura(void)
{
    int raw = 0;
    if (adc_oneshot_read(s_adc_handle, ADC_CH_NTC, &raw) != ESP_OK) {
        ESP_LOGW(TAG, "Error leyendo ADC NTC");
        return 25.0f;   /* Valor de seguridad */
    }

    /* Limitar rango para evitar divisiones por cero */
    if (raw <= 0)             raw = 1;
    if (raw >= ADC_FULL_SCALE) raw = ADC_FULL_SCALE - 1;

    float resistencia = R_SERIE * ((float)ADC_FULL_SCALE / raw - 1.0f);

    float inv_T = (1.0f / (TEMP_NOMINAL_C + 273.15f))
                + (1.0f / NTC_BETA) * logf(resistencia / NTC_R_NOMINAL);

    return (1.0f / inv_T) - 273.15f;
}

/**
 * @brief Lee el potenciometro y devuelve el valor ADC crudo (0–4095).
 */
static int leer_pot(void)
{
    int raw = 0;
    if (adc_oneshot_read(s_adc_handle, ADC_CH_POT, &raw) != ESP_OK) {
        ESP_LOGW(TAG, "Error leyendo ADC POT");
        return 0;
    }
    return raw;
}

/* ─── Control de LEDs ───────────────────────── */

static void pwm_write(ledc_channel_t canal, uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, canal, duty);
    ledc_update_duty(LEDC_MODE, canal);
}

static void set_led1(uint8_t r, uint8_t g, uint8_t b)
{
    pwm_write(CH_L1_R, r);
    pwm_write(CH_L1_G, g);
    pwm_write(CH_L1_B, b);
}

static void set_led2(uint8_t r, uint8_t g, uint8_t b)
{
    pwm_write(CH_L2_R, r);
    pwm_write(CH_L2_G, g);
    pwm_write(CH_L2_B, b);
}

/**
 * @brief Parpadea LED1 con el color indicado como confirmación visual.
 */
static void parpadear_led1(uint8_t r, uint8_t g, uint8_t b, int veces)
{
    for (int i = 0; i < veces; i++) {
        set_led1(r, g, b);
        vTaskDelay(pdMS_TO_TICKS(120));
        set_led1(0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
}

/* ═════════════════════════════════════════════
   UTILIDADES DE COLOR
   ═════════════════════════════════════════════ */

/**
 * @brief Convierte color HSV a RGB (componentes 0–255).
 * @param h Tono     0–360°
 * @param s Saturación 0.0–1.0
 * @param v Valor      0.0–1.0
 */
static rgb_t hsv_a_rgb(float h, float s, float v)
{
    float c  = v * s;
    float x  = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m  = v - c;
    float rf, gf, bf;

    if      (h <  60.0f) { rf = c; gf = x; bf = 0; }
    else if (h < 120.0f) { rf = x; gf = c; bf = 0; }
    else if (h < 180.0f) { rf = 0; gf = c; bf = x; }
    else if (h < 240.0f) { rf = 0; gf = x; bf = c; }
    else if (h < 300.0f) { rf = x; gf = 0; bf = c; }
    else                  { rf = c; gf = 0; bf = x; }

    return (rgb_t){
        .r = (uint8_t)((rf + m) * 255.0f),
        .g = (uint8_t)((gf + m) * 255.0f),
        .b = (uint8_t)((bf + m) * 255.0f),
    };
}

/**
 * @brief Calcula el color combinado promediando los 3 colores guardados.
 *        Este es el color que muestra LED2 en el Programa 1 y en el Estado 4.
 */
static rgb_t calcular_combinado(void)
{
    return (rgb_t){
        .r = (uint8_t)(((int)color_rojo.r + color_azul.r + color_verde.r) / 3),
        .g = (uint8_t)(((int)color_rojo.g + color_azul.g + color_verde.g) / 3),
        .b = (uint8_t)(((int)color_rojo.b + color_azul.b + color_verde.b) / 3),
    };
}

/* ═════════════════════════════════════════════
   PERSISTENCIA NVS (memoria no volátil)
   ═════════════════════════════════════════════ */

static void guardar_colores(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("rgb_cfg", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open error: %s", esp_err_to_name(err));
        return;
    }

    ESP_ERROR_CHECK(nvs_set_blob(h, "rojo",  &color_rojo,  sizeof(rgb_t)));
    ESP_ERROR_CHECK(nvs_set_blob(h, "azul",  &color_azul,  sizeof(rgb_t)));
    ESP_ERROR_CHECK(nvs_set_blob(h, "verde", &color_verde, sizeof(rgb_t)));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    ESP_LOGI(TAG, "  [NVS] Colores guardados en flash");
}

static void cargar_colores(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("rgb_cfg", NVS_READONLY, &h);
    if (err != ESP_OK) {
        /* Primera ejecución — namespace aún no existe */
        ESP_LOGI(TAG, "[NVS] Sin datos previos, usando valores por defecto");
        return;
    }

    size_t sz = sizeof(rgb_t);
    if (nvs_get_blob(h, "rojo", &color_rojo, &sz) == ESP_OK) {
        sz = sizeof(rgb_t); nvs_get_blob(h, "azul",  &color_azul,  &sz);
        sz = sizeof(rgb_t); nvs_get_blob(h, "verde", &color_verde, &sz);
        ESP_LOGI(TAG, "[NVS] Colores cargados:"
                      " Rojo(%d,%d,%d) Azul(%d,%d,%d) Verde(%d,%d,%d)",
                 color_rojo.r,  color_rojo.g,  color_rojo.b,
                 color_azul.r,  color_azul.g,  color_azul.b,
                 color_verde.r, color_verde.g, color_verde.b);
    } else {
        ESP_LOGI(TAG, "[NVS] Sin datos previos, usando valores por defecto");
    }

    nvs_close(h);
}

/* ═════════════════════════════════════════════
   DEBOUNCE DE BOTONES
   ═════════════════════════════════════════════
   Implementación basada en máquina de estados de dos bits.
   Devuelve true UNA SOLA VEZ por cada flanco de bajada (presión).
   Usa esp_timer_get_time() (microsegundos, 64 bits) para precisión.
   ═════════════════════════════════════════════ */

static bool btn_select_presionado(void)
{
    static bool   prev_lectura  = true;   /* true = HIGH (pull-up activo) */
    static bool   estado_estable = true;
    static int64_t t_debounce   = 0;

    bool lectura = (bool)gpio_get_level(PIN_BTN_SELECT); /* LOW al presionar */

    if (lectura != prev_lectura) {
        t_debounce  = esp_timer_get_time();
        prev_lectura = lectura;
    }

    if ((esp_timer_get_time() - t_debounce) > (DEBOUNCE_MS * 1000LL)) {
        if (lectura != estado_estable) {
            estado_estable = lectura;
            if (!estado_estable) {        /* Flanco de bajada = pulsado */
                return true;
            }
        }
    }
    return false;
}

static bool btn_mode_presionado(void)
{
    static bool   prev_lectura   = true;
    static bool   estado_estable = true;
    static int64_t t_debounce    = 0;

    bool lectura = (bool)gpio_get_level(PIN_BTN_MODE);

    if (lectura != prev_lectura) {
        t_debounce   = esp_timer_get_time();
        prev_lectura = lectura;
    }

    if ((esp_timer_get_time() - t_debounce) > (DEBOUNCE_MS * 1000LL)) {
        if (lectura != estado_estable) {
            estado_estable = lectura;
            if (!estado_estable) {
                return true;
            }
        }
    }
    return false;
}

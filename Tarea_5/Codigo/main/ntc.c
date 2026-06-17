#include "ntc.h"

#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "esp_log.h"

#define TAG "NTC"

// =====================================================
// HANDLES ADC (privados a este modulo)
// =====================================================

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t         cali_handle = NULL;
static int                       cali_ok     = 0;

// =====================================================
// ADC INIT
// Configura NTC (CH0) y potenciometro (CH1)
// Intenta calibracion curve fitting con eFuse
// =====================================================

void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_NTC_CHANNEL, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_POT_CHANNEL, &chan_cfg));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };

    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);

    if (ret == ESP_OK)
    {
        cali_ok = 1;
        ESP_LOGI(TAG, "Calibracion ADC OK");
    }
    else
    {
        cali_ok = 0;
        ESP_LOGW(TAG, "Sin calibracion ADC (%s)", esp_err_to_name(ret));
    }
}

// =====================================================
// LEER ADC MV
// Promedia ADC_SAMPLES lecturas y retorna milivolts
// =====================================================

int leer_adc_mv(adc_channel_t canal)
{
    int suma = 0;
    int raw  = 0;

    for (int i = 0; i < ADC_SAMPLES; i++)
    {
        adc_oneshot_read(adc_handle, canal, &raw);
        suma += raw;
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    int promedio = suma / ADC_SAMPLES;

    if (cali_ok)
    {
        int mv = 0;
        adc_cali_raw_to_voltage(cali_handle, promedio, &mv);
        return mv;
    }
    else
    {
        // Fallback lineal si no hay eFuse de calibracion
        return (int)((promedio / 4095.0f) * 3300.0f);
    }
}

// =====================================================
// LEER TEMPERATURA
// Circuito: 3.3V -> NTC -> ADC -> R_serie(10K) -> GND
// El ADC mide el voltaje sobre R_serie (parte baja).
// A mayor temperatura, NTC baja su resistencia,
// sube el voltaje en ADC -> temperatura sube.
//
// R_NTC = R_serie * (Vcc - Vadc) / Vadc
// =====================================================

float leer_temperatura(void)
{
    int mv = leer_adc_mv(ADC_NTC_CHANNEL);

    // Clamp: evita division por cero en los extremos
    // Con 10K+10K el punto medio es ~1650mV
    // Rango util practico: ~100mV (muy caliente) a ~3200mV (muy frio)
    if (mv <= 10)   mv = 10;
    if (mv >= 3290) mv = 3290;

    float voltaje     = mv / 1000.0f;

    // R_NTC: NTC esta en la parte alta del divisor (entre Vcc y ADC)
    float resistencia = NTC_SERIES_R * (3.3f - voltaje) / voltaje;

    // Ecuacion de Steinhart-Hart simplificada (beta)
    float steinhart;
    steinhart  = resistencia / NTC_NOMINAL_R;
    steinhart  = log(steinhart);
    steinhart /= NTC_BETA;
    steinhart += 1.0f / (NTC_NOMINAL_T + 273.15f);
    steinhart  = 1.0f / steinhart;
    steinhart -= 273.15f;

    return steinhart;
}

// =====================================================
// TEMPERATURE TASK
// Lee la temperatura cada 500ms y actualiza cfg.
// Imprime tambien el voltaje ADC para calibracion.
// =====================================================

void temperature_task(void *pvParameters)
{
    system_config_t *cfg = (system_config_t *)pvParameters;

    while (1)
    {
        int   mv   = leer_adc_mv(ADC_NTC_CHANNEL);
        float temp = leer_temperatura();

        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        cfg->temperatura = temp;
        xSemaphoreGive(cfg->mutex);

        // Voltaje impreso para verificar divisor y calibracion ADC
        ESP_LOGI(TAG, "ADC: %d mV | Temperatura: %.2f C", mv, temp);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
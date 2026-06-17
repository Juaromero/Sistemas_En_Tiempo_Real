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
// ADC INIT — NTC en CH0, Potenciometro en CH1
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
// LEER ADC MV — promedia ADC_SAMPLES lecturas
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
        return (int)((promedio / 4095.0f) * 3300.0f);
    }
}

// =====================================================
// LEER TEMPERATURA — retorna siempre en Celsius
// =====================================================

float leer_temperatura(void)
{
    int mv = leer_adc_mv(ADC_NTC_CHANNEL);

    if (mv <= 10)   mv = 10;
    if (mv >= 3290) mv = 3290;

    float voltaje     = mv / 1000.0f;
    float resistencia = NTC_SERIES_R * (3.3f - voltaje) / voltaje;

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
// CONVERTIR TEMPERATURA — de Celsius a la unidad activa
// =====================================================

float convertir_temperatura(float temp_c, int unidad)
{
    switch (unidad)
    {
        case UNIDAD_FAHRENHEIT: return (temp_c * 9.0f / 5.0f) + 32.0f;
        case UNIDAD_KELVIN:     return temp_c + 273.15f;
        case UNIDAD_CELSIUS:
        default:                return temp_c;
    }
}

// =====================================================
// SIMBOLO DE UNIDAD
// =====================================================

const char *simbolo_unidad(int unidad)
{
    switch (unidad)
    {
        case UNIDAD_FAHRENHEIT: return "F";
        case UNIDAD_KELVIN:     return "K";
        case UNIDAD_CELSIUS:
        default:                return "C";
    }
}

// =====================================================
// TEMPERATURE TASK
// =====================================================

void temperature_task(void *pvParameters)
{
    system_config_t *cfg = (system_config_t *)pvParameters;

    while (1)
    {
        // [FIX] El original leía leer_adc_mv() una vez para guardar en mv_ntc
        //       y luego llamaba leer_temperatura() que internamente volvía a
        //       llamar leer_adc_mv() — es decir, se realizaban DOS muestreos
        //       ADC separados (20 ms cada uno) y se mostraba el valor del
        //       primero mientras se calculaba la temperatura con el segundo.
        //       Ahora se llama leer_temperatura() primero (que ya promedia ADC)
        //       y se imprime con un único valor coherente.
        float temp_c = leer_temperatura();

        xSemaphoreTake(cfg->mutex, portMAX_DELAY);
        cfg->temperatura = temp_c;
        int unidad       = cfg->unidad;
        int fan_pct      = cfg->fan_duty_actual;
        xSemaphoreGive(cfg->mutex);

        float temp_conv = convertir_temperatura(temp_c, unidad);

        ESP_LOGI(TAG, "Temperatura: %.2f %s | Fan PWM: %d%%",
                 temp_conv, simbolo_unidad(unidad), fan_pct);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
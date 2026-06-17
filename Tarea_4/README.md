# Tarea 4 — Sensor NTC + ADC + NVS + Conversion HSV-RGB

Sistema dual con dos programas alternables por boton: **(1)** monitor de temperatura con LED indicador y **(2)** configurador de colores con potenciometro y persistencia en NVS.

## Descripcion

### Programa 1 — Monitor de Temperatura

Lee un termistor **NTC 10K** mediante ADC y colorea un LED RGB segun el rango de temperatura:

| Rango | Color LED1 |
|---|---|
| < 25 C | Azul (frio) |
| 25 - 35 C | Verde (confort) |
| > 35 C | Rojo (caliente) |

El LED2 muestra simultaneamente el color combinado guardado en el Programa 2.

### Programa 2 — Configuracion de Colores

Permite definir 3 colores personalizados usando el potenciometro como selector de tono (conversion HSV a RGB):

| Estado | Accion |
|---|---|
| 1 | Potenciometro define color ROJO -> BTN_SELECT guarda |
| 2 | Potenciometro define color AZUL -> BTN_SELECT guarda |
| 3 | Potenciometro define color VERDE -> BTN_SELECT guarda |
| 4 | LED2 muestra la mezcla de los 3 colores guardados |

- **LED1** actua como indicador de estado (color tenue del canal que se esta configurando).
- **LED2** muestra la previsualizacion del color en tiempo real.
- Los colores se **persisten en NVS** (Non-Volatile Storage) y se recuperan al reiniciar.

## Conceptos aplicados

- **ADC Oneshot** (`adc_oneshot.h`) para NTC y potenciometro
- **Ecuacion Steinhart-Hart** (Beta 3950 K, NTC 10K, R serie 10K)
- **PWM con LEDC** (8 bits, 5 kHz, 6 canales para 2 LEDs RGB)
- **NVS** para persistencia de colores (`nvs_set_blob`, `nvs_get_blob`)
- **Conversion HSV a RGB** para seleccion intuitiva de color con potenciometro
- **Debounce por maquina de estados** con `esp_timer_get_time()` (microsegundos)
- **Dos LEDs RGB** independientes (LED1 = indicador, LED2 = color)

## Pinout

| Pin | Funcion |
|---|---|
| GPIO2 | ADC - NTC (3.3V -> NTC -> GPIO2 -> R10K -> GND) |
| GPIO3 | ADC - Potenciometro |
| GPIO4 | Boton SELECT (guarda color) |
| GPIO5 | Boton MODE (alterna programa) |
| GPIO6 | LED1 Rojo |
| GPIO7 | LED1 Verde |
| GPIO8 | LED1 Azul |
| GPIO10 | LED2 Rojo |
| GPIO11 | LED2 Verde |
| GPIO12 | LED2 Azul |

## Estructura

```
Tarea_4/
├── Codigo/
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c
├── Documentos/
└── Imagenes/
```

## Compilar y flashear

```bash
idf.py set-target esp32c6
idf.py build
idf.py -p COMx flash monitor
```

# Tarea 3 — Control PWM de LED RGB con LEDC

Control de la intensidad de cada canal (R, G, B) de un LED RGB mediante **tres botones fisicos** y PWM generado con el periferico **LEDC** del ESP32.

## Descripcion

Cada boton incrementa la intensidad de su canal correspondiente en **10%**. Al llegar a 100% vuelve a 0%, creando un ciclo completo. La intensidad se controla por PWM a 4 kHz con resolucion de 13 bits.

### Funcionamiento

1. Se presiona el boton rojo -> la intensidad del canal R sube 10%.
2. Se presiona el boton verde -> la intensidad del canal G sube 10%.
3. Se presiona el boton azul -> la intensidad del canal B sube 10%.
4. Cada pulsacion imprime el estado actual por consola: `R: 30%  G: 0%  B: 50%`.

## Conceptos aplicados

- **PWM con LEDC** (`ledc_timer_config`, `ledc_channel_config`, `ledc_set_duty`)
- **Resolucion de 13 bits** (duty 0-8191)
- **Deteccion de flanco** por software (transicion 1 -> 0)
- **Libreria custom** (`library_led_c`) con estructura `led_rgb_t` reutilizable
- Antirrebote basico con `vTaskDelay(20ms)`

## Libreria `library_led_c`

Encapsula la configuracion y control del LED RGB:

| Funcion | Descripcion |
|---|---|
| `config_led_rgb()` | Configura timer y canales LEDC |
| `set_led_rgb_percentage_given_values()` | Establece intensidad por porcentaje (0-100) |
| `set_led_rgb_given_values()` | Establece duty crudo directamente |
| `increment_led_color()` | Incrementa un canal en 10% (ciclo 0-100) |

## Pinout

| Pin | Funcion |
|---|---|
| GPIO7 | LED RGB - Rojo |
| GPIO18 | LED RGB - Verde |
| GPIO19 | LED RGB - Azul |
| GPIO2 | Boton Rojo (entrada con pull-up) |
| GPIO3 | Boton Verde (entrada con pull-up) |
| GPIO4 | Boton Azul (entrada con pull-up) |

## Estructura

```
Tarea_3/
├── Codigo/
│   └── main/
│       ├── CMakeLists.txt
│       ├── ledc_basic_example_main.c
│       ├── library_led_c.c
│       └── library_led_c.h
├── Documentos/
└── Imagenes/
```

## Compilar y flashear

```bash
idf.py set-target esp32c6
idf.py build
idf.py -p COMx flash monitor
```

# Parcial 1 — Sistema Multi-Modo con Unidades y Temporizacion

Examen parcial que extiende la arquitectura de la Tarea 5 con soporte para **multiples unidades de temperatura** (C/F/K), **intervalo de impresion configurable** y **modo potenciometro con umbrales**.

## Descripcion

### Modos de operacion

| Modo | Nombre | Descripcion |
|---|---|---|
| 2 | Temperatura UART | LED RGB segun rangos configurables por UART |
| 3 | Intensidad UART | Control manual de intensidad R/G/B (0-100%) |
| 4 | Potenciometro Umbral | Color segun posicion del potenciometro (0-33% rojo, 34-66% verde, 67-100% azul) |

El boton BTN_MODO (GPIO4) cicla entre los modos 2, 3 y 4. El boton BOOT (GPIO9) cicla la unidad de temperatura: C -> F -> K -> C.

### Protocolo UART (115200 baud)

**Disponibles en cualquier modo:**
```
GET TEMP                              Temperatura en la unidad activa
SET UNIT <C|F|K>                      Cambia unidad de temperatura
SET INTERVAL <1-60>                   Intervalo de impresion (segundos)
```

**Modo 2 — Rangos configurables:**
```
SET LIM <RED|GREEN|BLUE> <min> <max>  Valores en la unidad activa
```

**Modo 3 — Intensidad manual:**
```
SET INT <RED|GREEN|BLUE> <0-100>
```

### Diferencias con Tarea 5

| Caracteristica | Tarea 5 | Parcial 1 |
|---|---|---|
| Unidades de temperatura | Solo Celsius | Celsius, Fahrenheit, Kelvin |
| Boton BOOT (GPIO9) | No usado | Cicla unidad de temperatura |
| Intervalo de impresion | Fijo | Configurable 1-60 s via UART |
| Modo 4 | Config. color con pot + guardar | Umbrales fijos (33%/66%) |
| Modos disponibles | 1, 2, 3, 4 | 2, 3, 4 |
| ESP-IDF | v5.5 | v6.0 |

## Conceptos aplicados

- **Conversion de unidades** de temperatura (Celsius, Fahrenheit, Kelvin)
- **Umbrales de potenciometro** para clasificacion de color
- **Intervalo configurable** de impresion por UART
- **4 tareas FreeRTOS** con mutex compartido
- **Protocolo UART** con parser de comandos extendido
- **Arquitectura modular** (misma estructura que Tarea 5)

## Arquitectura de tareas

```
Prioridad 6  ──  uart_task         (comandos UART)
Prioridad 4  ──  buttons_task      (pulsaciones de botones)
Prioridad 3  ──  rgb_task          (actualiza LED)
Prioridad 3  ──  temperature_task  (lee NTC + pot, imprime segun intervalo)
```

## Pinout

| Pin | Funcion |
|---|---|
| GPIO0 | ADC - NTC |
| GPIO1 | ADC - Potenciometro |
| GPIO4 | Boton MODO (cicla modos 2-3-4) |
| GPIO9 | Boton BOOT (cicla unidad C/F/K) |
| GPIO6 | LED RGB - Rojo |
| GPIO7 | LED RGB - Verde |
| GPIO8 | LED RGB - Azul |

## Estructura

```
Parcial_1/
├── Codigo/
│   └── main/
│       ├── CMakeLists.txt
│       ├── main.c
│       ├── config.h
│       ├── buttons.c / buttons.h
│       ├── ntc.c / ntc.h
│       ├── rgb.c / rgb.h
│       └── uart_manager.c / uart_manager.h
├── Documentos/
└── Imagenes/
```

## Compilar y flashear

```bash
idf.py set-target esp32c6
idf.py build
idf.py -p COMx flash monitor
```

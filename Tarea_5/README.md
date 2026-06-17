# Tarea 5 — Sistema Multi-Modo con Protocolo UART

Sistema integrado con **4 modos operativos** controlados por comandos UART, arquitectura modular con archivos separados y configuracion compartida protegida por **mutex**.

## Descripcion

### Modos de operacion

| Modo | Nombre | Descripcion |
|---|---|---|
| 1 | Temperatura Fija | LED RGB segun rangos fijos de temperatura (< 25C azul, 25-35C verde, > 35C rojo) |
| 2 | Temperatura UART | Igual que Modo 1 pero con rangos configurables por comandos UART |
| 3 | Intensidad UART | Control manual de intensidad R/G/B por comandos UART (0-100%) |
| 4 | Potenciometro | Configuracion de color usando potenciometro y boton de guardar |

El boton BTN_MODO (GPIO4) cicla entre los modos. El boton BTN_GUARDAR (GPIO5) guarda el color en Modo 4.

### Protocolo UART (115200 baud)

**Disponibles en cualquier modo:**
```
GET TEMP                       Retorna temperatura actual
GET COLOR                      Retorna color guardado (Modo 4)
```

**Modo 2 — Rangos configurables:**
```
SET LIM RED   <min> <max>      Limites en grados Celsius
SET LIM GREEN <min> <max>
SET LIM BLUE  <min> <max>
```

**Modo 3 — Intensidad manual:**
```
SET INT RED   <0-100>
SET INT GREEN <0-100>
SET INT BLUE  <0-100>
```

**Respuestas:**
```
OK <comando>
TEMP <valor> C
POT R:<val> G:<val> B:<val>
ERROR <motivo>
```

## Conceptos aplicados

- **UART** (`driver/uart.h`, 115200 baud) con parser de comandos
- **4 tareas FreeRTOS** con distintas prioridades
- **Mutex** (`xSemaphoreCreateMutex`) para proteger la estructura compartida
- **Arquitectura modular**: archivos `.c/.h` separados por funcionalidad
- **NTC** con ecuacion Steinhart-Hart (Beta 4100 K)
- **ADC** para NTC y potenciometro
- **PWM con LEDC** para LED RGB

## Arquitectura de tareas

```
Prioridad 6  ──  uart_task         (comandos UART)
Prioridad 4  ──  buttons_task      (pulsaciones de botones)
Prioridad 3  ──  rgb_task          (actualiza LED)
Prioridad 3  ──  temperature_task  (lee sensor NTC)
```

## Pinout

| Pin | Funcion |
|---|---|
| GPIO0 | ADC - NTC |
| GPIO1 | ADC - Potenciometro |
| GPIO4 | Boton MODO (cicla modos) |
| GPIO5 | Boton GUARDAR (guarda color en Modo 4) |
| GPIO6 | LED RGB - Rojo |
| GPIO7 | LED RGB - Verde |
| GPIO8 | LED RGB - Azul |

## Estructura

```
Tarea_5/
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

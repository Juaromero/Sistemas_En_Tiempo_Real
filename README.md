# Sistemas en Tiempo Real

Repositorio correspondiente a la materia de **Sistemas en Tiempo Real**, donde se recopilan las practicas, laboratorios y ejercicios desarrollados a lo largo del semestre.

## Plataforma

| Componente | Detalle |
|---|---|
| Microcontrolador | **ESP32-C6** (RISC-V) |
| Framework | **ESP-IDF v5.5** / v6.0 |
| RTOS | **FreeRTOS** |
| Toolchain | `riscv32-esp-elf-gcc` |
| Build System | CMake + Ninja |

## Estructura del repositorio

```
Sistemas_En_Tiempo_Real/
├── Tarea_1/           Polling de botones + tareas FreeRTOS + colas
├── Tarea_2/           Interrupciones GPIO (ISR) + antirrebote por software
├── Tarea_3/           Control PWM de LED RGB con LEDC
├── Tarea_4/           Sensor NTC + ADC + NVS + conversion HSV-RGB
├── Tarea_5/           Sistema multi-modo con protocolo UART
├── Parcial_1/         Examen parcial (variante de Tarea 5)
└── Proyecto_Final/    Sistema IoT completo de control ambiental
```

Cada carpeta contiene:

```
<Proyecto>/
├── Codigo/            Proyecto ESP-IDF compilable
│   └── main/          Codigo fuente de la aplicacion
├── Documentos/        Documentacion adicional
└── Imagenes/          Diagramas e imagenes de referencia
```

## Contenido por tarea

### Tarea 1 — Polling + Colas

Cicla patrones de parpadeo de un LED mediante un boton, usando **polling** en una tarea FreeRTOS y comunicacion por **colas** (`xQueueSend` / `xQueueReceive`).

### Tarea 2 — Interrupciones (ISR)

Misma funcionalidad que la Tarea 1, pero el boton se atiende mediante una **interrupcion por flanco negativo** (`GPIO_INTR_NEGEDGE`) con antirrebote de 200 ms y cola segura para ISR (`xQueueSendFromISR`).

### Tarea 3 — PWM con LEDC

Control de un LED RGB (catodo comun) con **PWM** usando el periferico LEDC del ESP32. Tres botones permiten incrementar la intensidad de cada canal (R, G, B) independientemente.

### Tarea 4 — ADC + NTC + NVS

Sistema dual: **(1)** monitor de temperatura con termistor NTC (ecuacion Steinhart-Hart, Beta 4100 K) que colorea un LED segun rangos y **(2)** configuracion de colores con potenciometro. Los valores se persisten en **NVS** (Non-Volatile Storage).

### Tarea 5 — Multi-modo + UART

Sistema con 4 modos operativos controlados por un protocolo de comandos UART a 115200 baud (`SET UNIT`, `SET LIM`, `SET INT`, `GET TEMP`). Arquitectura modular con archivos `.c/.h` separados y un **mutex** para proteger la configuracion compartida entre tareas.

### Parcial 1

Variante del sistema de la Tarea 5 compilada con ESP-IDF v6.0.

### Proyecto Final — Sistema de Control Ambiental IoT

Sistema completo de automatizacion ambiental con los siguientes subsistemas:

| Subsistema | Descripcion |
|---|---|
| **Temperatura** | Sensor NTC con lectura ADC, conversion C/F/K |
| **Ventilador** | Motor 12 V controlado por MOSFET (IRLZ44N), PWM 25 kHz. Modo automatico (por temperatura) o manual |
| **Cortinas** | Servomotor (PWM 50 Hz, pulso 500-2500 us). Modo manual o programado por horario (hasta 8 registros) |
| **Iluminacion RGB** | LED RGB con brillo configurable via PWM |
| **Alarma** | LED parpadeante a 1 Hz cuando la temperatura supera el maximo |
| **WiFi** | Modo dual AP + STA con credenciales guardadas en NVS |
| **Panel Web** | Dashboard HTML/CSS/JS (jQuery) embebido en flash, servido por HTTP |
| **API REST** | Endpoints GET/POST para status, fan, servo, horarios, RGB y WiFi |
| **OTA** | Actualizacion de firmware over-the-air desde el navegador |
| **NTP** | Sincronizacion de hora para los horarios automaticos del servo |

#### Arquitectura de tareas (FreeRTOS)

```
Prioridad 5  ──  wifi_app_task        (core 0)
Prioridad 4  ──  http_server_task     (core 0)
Prioridad 3  ──  rgb_task / temperature_task / fan_task
Prioridad 2  ──  servo_task
```

Todas las tareas comparten una estructura `system_config_t` protegida por un **mutex**.

#### Pinout (ESP32-C6)

| Pin | Funcion |
|---|---|
| GPIO0 | ADC — NTC |
| GPIO1 | ADC — Potenciometro |
| GPIO2 | LED Alarma |
| GPIO4 | Boton Modo |
| GPIO6, 7, 8 | LED RGB (R, G, B) |
| GPIO9 | Boton BOOT (cicla unidad) |
| GPIO10 | LED de estado (blink) |
| GPIO15 | Fan (gate MOSFET) |
| GPIO20 | Servo (senal PWM) |

#### Endpoints de la API

| Metodo | Ruta | Descripcion |
|---|---|---|
| `GET` | `/api/status` | Estado completo del sistema (JSON) |
| `POST` | `/api/fan` | Configurar ventilador (modo, temperaturas, velocidad) |
| `POST` | `/api/servo` | Configurar servo (modo, porcentaje manual) |
| `GET` | `/api/servo/schedule` | Obtener los 8 registros de horario |
| `POST` | `/api/servo/schedule` | Actualizar un registro de horario |
| `POST` | `/api/rgb` | Configurar color e intensidad del LED RGB |
| `POST` | `/api/wifi/sta` | Guardar credenciales de red WiFi (STA) |
| `POST` | `/api/wifi/ap` | Configurar punto de acceso (AP) |
| `POST` | `/OTAupdate` | Subir firmware (.bin) |
| `POST` | `/OTAstatus` | Consultar estado de la actualizacion OTA |

## Compilar y flashear

```bash
# Configurar el target
idf.py set-target esp32c6

# Compilar
idf.py build

# Flashear y abrir monitor serial
idf.py -p COMx flash monitor
```

## Requisitos

- [ESP-IDF v5.5+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/)
- ESP32-C6 DevKit
- Cable USB-C

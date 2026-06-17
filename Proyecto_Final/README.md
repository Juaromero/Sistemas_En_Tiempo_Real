# Proyecto Final — Sistema de Control Ambiental IoT (STR 2026)

Sistema completo de automatizacion ambiental sobre ESP32-C6 con **panel web embebido**, **WiFi dual (AP+STA)**, **actualizacion OTA** y multiples subsistemas de control en tiempo real.

## Descripcion general

El sistema integra sensores, actuadores y una interfaz web para controlar el ambiente de una habitacion:

| Subsistema | Descripcion |
|---|---|
| **Temperatura** | Sensor NTC 10K con lectura ADC, conversion C/F/K |
| **Ventilador** | Motor 12 V controlado por MOSFET IRLZ44N, PWM 25 kHz |
| **Cortinas** | Servomotor con programacion horaria (hasta 8 registros) |
| **Iluminacion RGB** | LED RGB con brillo global configurable |
| **Alarma** | LED parpadeante a 1 Hz cuando la temperatura supera el maximo |
| **WiFi** | Modo dual AP + STA con credenciales en NVS |
| **Panel Web** | Dashboard HTML/CSS/JS embebido en flash |
| **OTA** | Actualizacion de firmware desde el navegador |
| **NTP** | Sincronizacion horaria para horarios automaticos |

## Panel web

El ESP32 sirve un dashboard accesible desde cualquier navegador conectado a su red. Permite controlar todos los subsistemas sin necesidad de aplicaciones externas.

### Secciones del dashboard

- **Barra de estado**: temperatura actual, duty del ventilador, posicion de cortina, estado de alarma (polling cada 3 s)
- **Ventilador**: modo automatico (con temperatura deseada/maxima) o manual (slider 0-100%)
- **Cortinas**: modo manual (slider 0-100%) o por horario (tabla de 8 registros con hora, minuto, porcentaje y activacion)
- **Iluminacion RGB**: sliders R/G/B (0-255) con preview de color + slider de brillo global
- **WiFi**: configuracion de credenciales STA (conectar a router) y AP (punto de acceso propio)
- **Firmware**: actualizacion OTA subiendo archivo .bin

## Ventilador

### Modo automatico (`fan_modo = 0`)

Control proporcional lineal basado en temperatura:

```
temp <= temp_deseada                ->  0% (apagado)
temp_deseada < temp < temp_maxima   ->  proporcional (0-100%)
temp >= temp_maxima                 ->  100% + ALARMA
```

### Modo manual (`fan_modo = 1`)

Velocidad fija definida por el usuario (0-100%). La alarma se activa igualmente si la temperatura supera el maximo.

## Cortinas (Servo)

### Modo manual (`servo_modo = 1`)

Apertura directa con slider (0% = cerrado, 100% = abierto).

### Modo horario (`servo_modo = 0`)

Hasta **8 registros programables**, cada uno con:
- **Hora** (0-23)
- **Minuto** (0-59)
- **Porcentaje** de apertura (0-100%)
- **Activo** (habilitado/deshabilitado)

La tarea `servo_task` revisa la hora del sistema (sincronizada por NTP) cada 250 ms y ejecuta el horario cuando coincide.

## API REST

| Metodo | Ruta | Descripcion |
|---|---|---|
| `GET` | `/api/status` | Estado completo del sistema (JSON) |
| `POST` | `/api/fan` | Configurar ventilador |
| `POST` | `/api/servo` | Configurar servo (modo, porcentaje manual) |
| `GET` | `/api/servo/schedule` | Obtener los 8 registros de horario |
| `POST` | `/api/servo/schedule` | Actualizar un registro de horario |
| `POST` | `/api/rgb` | Configurar color e intensidad del LED RGB |
| `POST` | `/api/wifi/sta` | Guardar credenciales WiFi STA |
| `POST` | `/api/wifi/ap` | Configurar punto de acceso AP |
| `POST` | `/OTAupdate` | Subir firmware (.bin) |
| `POST` | `/OTAstatus` | Consultar estado de la actualizacion OTA |

### Ejemplos de cuerpo JSON

**Ventilador:**
```json
{"modo": 0, "temp_deseada": 25.0, "temp_maxima": 35.0}
{"modo": 1, "manual_pct": 50}
```

**Horario de cortinas:**
```json
{"index": 0, "hora": 8, "minuto": 0, "porcentaje": 100, "activo": 1}
{"index": 1, "hora": 20, "minuto": 0, "porcentaje": 0, "activo": 1}
```

**RGB:**
```json
{"red": 255, "green": 128, "blue": 0, "brillo": 80}
```

## Arquitectura de tareas (FreeRTOS)

```
Prioridad 5  ──  wifi_app_task          (core 0) — WiFi AP+STA
Prioridad 4  ──  http_server_task       (core 0) — Servidor HTTP + API
Prioridad 3  ──  http_server_monitor    (core 0) — Eventos del servidor
Prioridad 3  ──  rgb_task               — LED RGB
Prioridad 3  ──  temperature_task       — Lectura NTC
Prioridad 3  ──  fan_task               — Control ventilador + alarma
Prioridad 2  ──  servo_task             — Control cortinas
```

Todas las tareas comparten una estructura `system_config_t` protegida por un **mutex FreeRTOS**.

## Pinout (ESP32-C6)

| Pin | Funcion |
|---|---|
| GPIO0 | ADC - NTC (sensor de temperatura) |
| GPIO1 | ADC - Potenciometro |
| GPIO2 | LED Alarma (parpadeo 1 Hz) |
| GPIO4 | Boton Modo |
| GPIO6 | LED RGB - Rojo |
| GPIO7 | LED RGB - Verde |
| GPIO8 | LED RGB - Azul |
| GPIO9 | Boton BOOT (cicla unidad de temperatura) |
| GPIO10 | LED de estado (blink) |
| GPIO15 | Ventilador (gate MOSFET IRLZ44N) |
| GPIO20 | Servomotor (senal PWM 50 Hz) |

## Conexion WiFi

Al encender, el ESP32 crea un punto de acceso:

| Parametro | Valor |
|---|---|
| SSID | `STR_2026` |
| Password | `str2026pass` |
| IP | `192.168.4.1` |

Desde el panel web se pueden configurar credenciales STA para conectar a un router existente.

## Estructura

```
Proyecto_Final/
├── Codigo/
│   ├── CMakeLists.txt
│   ├── sdkconfig
│   ├── partitions.csv
│   └── main/
│       ├── CMakeLists.txt
│       ├── main.c              Punto de entrada (app_main)
│       ├── config.h            Pines, constantes, estructuras
│       ├── tasks_common.h      Prioridades y stacks de tareas
│       ├── ntc.c / ntc.h       Sensor NTC (Steinhart-Hart)
│       ├── rgb.c / rgb.h       Control LED RGB (PWM LEDC)
│       ├── rgb_led.c / rgb_led.h   LED indicador de estado WiFi
│       ├── fan.c / fan.h       Ventilador (PWM 25 kHz + alarma)
│       ├── servo.c / servo.h   Servomotor (PWM 50 Hz + horarios)
│       ├── wifi_app.c / wifi_app.h   WiFi AP+STA + NTP
│       ├── http_server.c / http_server.h   Servidor HTTP + API REST + OTA
│       └── webpage/
│           ├── index.html      Dashboard HTML
│           ├── app.css         Estilos
│           ├── app.js          Logica del frontend (jQuery)
│           ├── jquery-3.3.1.min.js
│           └── favicon.ico
├── Documentos/
└── Imagenes/
```

## Compilar y flashear

```bash
idf.py set-target esp32c6
idf.py build
idf.py -p COMx flash monitor
```

## Requisitos

- [ESP-IDF v5.5+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/)
- ESP32-C6 DevKit
- Ventilador 12 V + MOSFET IRLZ44N
- Servomotor (compatible con pulso 500-2500 us)
- Termistor NTC 10K + resistencia 10K
- LED RGB catodo comun + resistencias 220 ohm
- Potenciometro
- LED rojo para alarma

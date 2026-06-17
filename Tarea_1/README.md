# Tarea 1 — Polling de Botones + Colas FreeRTOS

Control de patrones de parpadeo de un LED mediante el boton BOOT, usando **polling** (sin interrupciones) y comunicacion entre tareas con **colas** de FreeRTOS.

## Descripcion

El sistema implementa dos tareas concurrentes:

- **`button_task`**: monitorea el boton BOOT por polling cada 10 ms. Cuando detecta una pulsacion, avanza al siguiente patron de parpadeo y envia el nuevo estado a traves de una cola.
- **`led_task`**: recibe el estado de la cola y ejecuta el patron de parpadeo correspondiente sobre el LED.

### Patrones de parpadeo

| Estado | Encendido | Apagado |
|---|---|---|
| `LED_2_2` | 2 s | 2 s |
| `LED_2_1` | 2 s | 1 s |
| `LED_1_1` | 1 s | 1 s |
| `LED_05_05` | 0.5 s | 0.5 s |
| `LED_OFF` | Apagado permanente | -- |

El ciclo se repite: al llegar a `LED_OFF` vuelve a `LED_2_2`.

## Conceptos aplicados

- **Polling** de GPIO con `gpio_get_level()`
- **Antirrebote** por software (espera activa hasta que el boton sea soltado)
- **Colas FreeRTOS** (`xQueueCreate`, `xQueueSend`, `xQueueReceive`)
- **Tareas FreeRTOS** (`xTaskCreate`, `vTaskDelay`)
- Configuracion de GPIO como entrada (pull-up) y salida

## Pinout

| Pin | Funcion |
|---|---|
| GPIO2 | LED externo (salida) |
| GPIO9 | Boton BOOT (entrada con pull-up) |

## Estructura

```
Tarea_1/
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

# Tarea 2 — Interrupciones GPIO (ISR)

Control de patrones de parpadeo de un LED mediante el boton BOOT, usando **interrupciones por hardware** (ISR) en lugar de polling.

## Descripcion

A diferencia de la Tarea 1, el boton ya no se monitorea en un bucle. En su lugar se configura una **interrupcion por flanco negativo** (`GPIO_INTR_NEGEDGE`) que dispara automaticamente una funcion ISR cada vez que el boton es presionado.

### Arquitectura

- **ISR (`button_isr_handler`)**: se ejecuta en RAM (`IRAM_ATTR`) cuando el boton genera un flanco de bajada. Implementa antirrebote por tiempo (200 ms) y envia el nuevo estado a la cola con `xQueueSendFromISR`.
- **`led_task`**: unica tarea, recibe el estado de la cola y ejecuta el patron de parpadeo.

Ya no existe `button_task` — el boton se maneja completamente por interrupcion.

### Patrones de parpadeo

| Estado | Encendido | Apagado |
|---|---|---|
| `LED_2_2` | 2 s | 2 s |
| `LED_2_1` | 2 s | 1 s |
| `LED_1_1` | 1 s | 1 s |
| `LED_05_05` | 0.5 s | 0.5 s |
| `LED_OFF` | Apagado permanente | -- |

## Conceptos aplicados

- **Interrupciones GPIO** (`gpio_install_isr_service`, `gpio_isr_handler_add`)
- **ISR en IRAM** (`IRAM_ATTR`) para ejecucion rapida
- **Antirrebote por tiempo** usando `xTaskGetTickCountFromISR()`
- **Cola segura para ISR** (`xQueueSendFromISR`)
- **Cambio de contexto desde ISR** (`portYIELD_FROM_ISR`)
- Flanco negativo (`GPIO_INTR_NEGEDGE`)

## Diferencias con Tarea 1

| Aspecto | Tarea 1 (Polling) | Tarea 2 (ISR) |
|---|---|---|
| Deteccion del boton | Lectura periodica cada 10 ms | Interrupcion por hardware |
| Tareas | `button_task` + `led_task` | Solo `led_task` |
| Antirrebote | Espera activa (while) | Comparacion de timestamps |
| Uso de CPU | Mayor (polling constante) | Menor (ISR solo cuando hay evento) |
| Envio a cola | `xQueueSend` | `xQueueSendFromISR` |

## Pinout

| Pin | Funcion |
|---|---|
| GPIO2 | LED externo (salida) |
| GPIO9 | Boton BOOT (entrada con pull-up, interrupcion NEGEDGE) |

## Estructura

```
Tarea_2/
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

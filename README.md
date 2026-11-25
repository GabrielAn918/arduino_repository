# Internet de las cosas — Arduino UNO en simulación

Repositorio público del trabajo del módulo **Internet de las cosas** (Politécnico Grancolombiano).
Autor: **Gabriel Andres Castillo Garcia**.

Contiene todo lo necesario para revisar y reproducir cada entrega: código fuente documentado,
archivos de configuración de los simuladores, bancos de prueba y registros originales de medición.

| Carpeta | Contenido |
|---|---|
| [`entrega1/`](entrega1/) | Uso práctico de Arduino: estado del arte, herramientas de simulación y monitoreo de un montaje *blink* con 2 entradas simuladas (Wokwi vs. simavr). |
| [`entrega2/`](entrega2/) | Recepción de comandos por Bluetooth (HC-05) en un Arduino UNO, enlace serie, simulación en Wokwi y banco automatizado en simavr. Incluye la validación de las hipótesis de la Entrega 1. |
| [`scripts/`](scripts/) | `compilar_firmware.sh`: compila cualquier sketch con avr-gcc y las mismas opciones del IDE de Arduino (con LTO), para obtener el mismo binario que compila Wokwi. |

## Requisitos (Linux, probado en Ubuntu 24.04)

```bash
sudo apt install gcc-avr avr-libc simavr libsimavr-dev git python3-pandas python3-matplotlib
```

El script de compilación descarga [ArduinoCore-avr](https://github.com/arduino/ArduinoCore-avr) la primera vez
(o use `CORE_DIR=/ruta/al/core`). Comprobación rápida:

```bash
scripts/compilar_firmware.sh entrega2/firmware/bt_leds/bt_leds.ino /tmp/bt.elf
# Program: 6356 bytes · Data: 409 bytes  (mismo tamaño que reporta Wokwi)
```

## Licencia

MIT (ver [`LICENSE`](LICENSE)). Los datos y figuras pueden reutilizarse citando el artículo correspondiente.

# Entrega 2 — Recepción por Bluetooth (HC-05) en Arduino UNO

Circuito: módulo HC-05 conectado a la UART del Arduino UNO (D0/D1) a 9600 baudios 8N1, pin STATE en D2,
pin EN en D7, y dos LED (verde en D5, rojo en D6, con resistencias de 220 Ω). El teléfono envía líneas
de texto por el perfil SPP; el Arduino las interpreta y responde.

- **Proyecto en Wokwi:** ENLACE_WOKWI
- **Artículo (IEEE):** [`articulo/`](articulo/)

## Estructura

| Carpeta | Qué contiene |
|---|---|
| `firmware/bt_leds/` | Firmware del Arduino, documentado (protocolo, conexiones, seguridad ante pérdida del enlace). |
| `wokwi/` | Proyecto completo de Wokwi: `sketch.ino`, `diagram.json` y el modelo propio del HC-05 (`hc05.chip.c`, `hc05.chip.json`) hecho con la API de chips personalizados. |
| `banco_simavr/` | Banco de pruebas automatizado (`bench_bt.c`) sobre simavr y `ejecutar_pruebas.sh` para repetir todas las pruebas. |
| `validacion_e1/` | Firmware de medición (`validacion_e1.ino`), arnés simavr (`valida_e1.c`) y `ejecutar.sh` para validar las hipótesis de la Entrega 1. |
| `datos/` | Registros originales: `simavr/` (CSV por comando, ráfagas, seguridad), `wokwi/` (consola de chips, monitor serie, latencias), `validacion/` (iteraciones/s, ciclos de ADC, factores de velocidad; `sin_lto/` guarda la primera corrida con el binario sin LTO). `resumen_e2.json` reúne las cifras del artículo. |
| `analisis/` | `analisis_e2.py` regenera tablas, `resumen_e2.json` y figuras; `diagrama_bloques.py` dibuja la Fig. 1. |
| `figuras/` | Figuras del artículo y capturas de Wokwi. |

## Cómo reproducir

**1. Simulación interactiva (Wokwi).** Abra el enlace del proyecto y pulse ▶. El monitor serie hace de
aplicación del teléfono: escriba `V1`, `R1`, `V=128`, `STATUS`, etc. El control **Enlace** del chip HC-05
(0/1) simula la pérdida y recuperación de la conexión. Para la autoprueba de 65 comandos, cambie en
`diagram.json` el atributo `"autotest": "0"` por `"1"`; los resultados aparecen en la *Chips Console*.

**2. Banco automatizado (simavr).**
```bash
cd entrega2/banco_simavr && ./ejecutar_pruebas.sh
```
Compila el firmware a 9600, 38400 y 115200 baudios (y con búfer de recepción de 256 B), ejecuta 2000
comandos aleatorios por velocidad contra el oráculo, las ráfagas y la prueba de seguridad. Escribe en `datos/simavr/`.
Modos individuales: `./bench_bt <firmware.elf> <baudios> <salida.csv> func <N> <semilla> <pausa_max_ms>`,
`... burst <K>` y `... failsafe`.

**3. Validación de la Entrega 1.**
```bash
cd entrega2/validacion_e1 && ./ejecutar.sh
```
Para Wokwi, pegue `validacion_e1.ino` en un proyecto con solo el Arduino UNO y copie la salida del monitor serie.

**4. Análisis y figuras.**
```bash
python3 entrega2/analisis/analisis_e2.py
```

## Resultados principales

| Prueba | Resultado |
|---|---|
| Wokwi, autoprueba | 65/65 respuestas correctas; 1,044 ms/byte (teórico 1,042) |
| simavr, 2000 comandos × 3 velocidades | 100 % correctos; latencia media 23,4 / 5,9 / 2,0 ms |
| Ráfagas sin control de flujo | pérdidas desde 35 comandos (búfer 64 B); ~100 con búfer de 256 B |
| Pérdida del enlace | LED apagados 0,018 ms después de que STATE baja |
| Hipótesis del ADC (E1) | Rechazada: 1674,7 vs 1674,4 ciclos; la diferencia venía de compilar sin LTO |
| Hipótesis del pull-up (E1) | Refinada: simavr se ralentiza (≈2× tiempo real) cuando D2 se fuerza a 0 desde fuera |

## Notas

- En hardware real, RXD del HC-05 es de 3,3 V: se requiere un divisor de 1 kΩ / 2 kΩ desde TX del Arduino.
  En Wokwi se conecta directo porque el simulador no modela niveles analógicos en la UART.
- D0/D1 se comparten con el USB: desconecte el HC-05 mientras carga el programa en una placa física.
- En el registro `datos/wokwi/autoprueba_chips_console.txt` la línea RESUMEN aparece una vez; durante esa
  corrida el modelo la repetía en cada tic y se corrigió después con la bandera `auto_done` (ya incluida en `hc05.chip.c`).
- simavr calcula cada trama UART con 11 bits (suma siempre un bit de paridad), por eso sus latencias son ~10 % mayores que las de Wokwi.

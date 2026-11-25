# Entrega 1 — Uso práctico de Arduino en simulación

Montaje *blink* en Arduino UNO con 5 LED y 2 entradas que simulan sensores: un potenciómetro en A0
(sensor analógico, define el periodo de parpadeo) y un interruptor en D2 (sensor digital, cambia entre
barrido y modo alarma). Se ejecutó en Wokwi y en simavr para comparar comportamiento, temporización
y consumo de recursos de cada simulador.

- **Artículo (IEEE):** [`articulo/`](articulo/)

| Carpeta | Qué contiene |
|---|---|
| `firmware/blink_5s_2e/` | Sketch del montaje. |
| `wokwi/` | `sketch.ino` y `diagram.json` del proyecto en Wokwi. |
| `banco_simavr/` | Arnés simavr (`harness.c`), medidor de recursos (`medir.py`) y `Makefile`. |
| `datos/` | Series del monitor serie y rendimiento de Wokwi, recursos de simavr (CPU, memoria) en tiempo real y a máxima velocidad. `crudos/` guarda las salidas originales (`top`, `vmstat`, registros del arnés). |
| `analisis/` | `analisis_e1.py` regenera las cifras y figuras (ejecutar desde `entrega1/`). |
| `figuras/` | Figuras del artículo. |

## Cómo reproducir

```bash
cd entrega1/banco_simavr && make medir
cd .. && python3 analisis/analisis_e1.py
```

## Nota posterior (Entrega 2)

La diferencia de rendimiento entre simuladores observada aquí se validó en la Entrega 2: se debía a que
el binario local se compiló sin LTO; con las opciones del IDE de Arduino ambos simuladores coinciden en
menos de 0,3 % (ver `../entrega2/README.md`).

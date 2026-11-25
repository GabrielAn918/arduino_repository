#!/usr/bin/env bash
# Validación de hipótesis de la Entrega 1 en simavr. Ejecutar desde entrega2/validacion_e1/
set -euo pipefail
R=../../scripts/compilar_firmware.sh; OUT=../datos/validacion; mkdir -p build "$OUT"
gcc -O2 -o valida_e1 valida_e1.c -lsimavr
$R validacion_e1.ino build/val_lto_PULLUP.elf -DMODO_D2=INPUT_PULLUP
$R validacion_e1.ino build/val_lto_INPUT.elf  -DMODO_D2=INPUT
# Hipótesis del ADC: mismo binario que Wokwi (con LTO)
for r in 1 2 3; do ./valida_e1 build/val_lto_PULLUP.elf 6 -1 $OUT/simavr_LTO_PULLUP_r$r.txt >/dev/null 2>&1; done
# Hipótesis del pull-up: 2 modos x 3 niveles (-1 = sin conexión) x 3 repeticiones, nivel aplicado a los 2 s


for M in PULLUP INPUT; do for D in -1 0 1; do for r in 1 2 3; do
  ./valida_e1 build/val_lto_$M.elf 12 $D $OUT/simavr_LTO_${M}_d${D}_r$r.txt 2 >/dev/null 2>&1; done; done; done
echo "Listo. Compare con $OUT/wokwi_validacion_PULLUP.txt"

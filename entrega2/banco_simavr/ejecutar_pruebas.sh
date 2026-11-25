#!/usr/bin/env bash
# Reproduce todas las pruebas del banco simavr de la Entrega 2.
# Requisitos: sudo apt install simavr libsimavr-dev gcc-avr avr-libc
# Ejecutar desde entrega2/banco_simavr/
set -euo pipefail
R=../../scripts/compilar_firmware.sh; FW=../firmware/bt_leds/bt_leds.ino; OUT=../datos/simavr; mkdir -p build "$OUT"
gcc -O2 -o bench_bt bench_bt.c -lsimavr
for B in 9600 38400 115200; do $R $FW build/bt_$B.elf -DBAUD_BT_CFG=$B; done
$R $FW build/bt_9600_rx256.elf -DBAUD_BT_CFG=9600 -DSERIAL_RX_BUFFER_SIZE=256
: > $OUT/bench_log.txt
for B in 9600 38400 115200; do ./bench_bt build/bt_$B.elf $B $OUT/func_$B.csv func 2000 7 20 2>&1 | grep -v Loaded | tee -a $OUT/bench_log.txt; done
echo "baud,K,respuestas,coinciden,perdidas,dur_ms,leds_ok" > $OUT/burst.csv
for B in 9600 38400 115200; do for K in 5 10 20 30 35 40 60 80 160 320; do ./bench_bt build/bt_$B.elf $B /tmp/b.csv burst $K 2>>$OUT/bench_log.txt | grep -v Loaded >> $OUT/burst.csv; done; done
echo "baud,K,respuestas,coinciden,perdidas,dur_ms,leds_ok" > $OUT/burst_rx256.csv
for K in 5 10 20 30 40 60 80 100 120 160 320; do ./bench_bt build/bt_9600_rx256.elf 9600 /tmp/b.csv burst $K 2>>$OUT/bench_log.txt | grep -v Loaded >> $OUT/burst_rx256.csv; done
echo "antes,despues,t_ms" > $OUT/failsafe.csv
./bench_bt build/bt_9600.elf 9600 /tmp/x.csv failsafe 2>>$OUT/bench_log.txt | grep -v Loaded >> $OUT/failsafe.csv
echo "Listo. Resultados en $OUT"

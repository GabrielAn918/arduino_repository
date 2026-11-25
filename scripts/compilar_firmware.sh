#!/usr/bin/env bash
# Compila un sketch de Arduino UNO con avr-gcc y las MISMAS opciones del IDE de Arduino
# (incluida la optimización en tiempo de enlace, LTO), para obtener un binario equivalente
# al que compila Wokwi. Probado con avr-gcc 7.3.0 en Ubuntu 24.04.
#
# Uso: scripts/compilar_firmware.sh <sketch.ino> <salida.elf> [defines extra, p. ej. -DBAUD_BT_CFG=38400]
# Requisitos: sudo apt install gcc-avr avr-libc git
set -euo pipefail
SKETCH="$1"; OUT="$2"; shift 2; EXTRA="${*:-}"
CORE_DIR="${CORE_DIR:-$HOME/.cache/ArduinoCore-avr}"
[ -d "$CORE_DIR" ] || git clone --depth 1 https://github.com/arduino/ArduinoCore-avr.git "$CORE_DIR"
C="$CORE_DIR/cores/arduino"; V="$CORE_DIR/variants/standard"
D="-mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10607 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR -I$C -I$V"
CF="-c -g -Os -w -std=gnu++11 -fpermissive -fno-exceptions -ffunction-sections -fdata-sections -fno-threadsafe-statics -Wno-error=narrowing -flto"
B=$(mktemp -d)
for f in "$C"/*.c;   do avr-gcc -c -g -Os -w -std=gnu11 -ffunction-sections -fdata-sections -flto -fno-fat-lto-objects $D $EXTRA "$f" -o "$B/$(basename "$f").o"; done
for f in "$C"/*.cpp; do avr-g++ $CF $D $EXTRA "$f" -o "$B/$(basename "$f").o"; done
avr-gcc -c -g -x assembler-with-cpp -flto $D "$C/wiring_pulse.S" -o "$B/wiring_pulse.S.o"
avr-gcc-ar rcs "$B/core.a" "$B"/*.o
{ echo '#include <Arduino.h>'; cat "$SKETCH"; } > "$B/sketch.cpp"
avr-g++ $CF $D $EXTRA "$B/sketch.cpp" -o "$B/sketch.o"
avr-gcc -w -Os -g -flto -fuse-linker-plugin -Wl,--gc-sections -mmcu=atmega328p -o "$OUT" "$B/sketch.o" "$B/core.a" -lm
avr-size -C --mcu=atmega328p "$OUT" | grep -E "Program|Data"
rm -rf "$B"

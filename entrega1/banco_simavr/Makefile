# Banco de pruebas de la Entrega 1 (simavr). Requisitos: simavr libsimavr-dev gcc-avr avr-libc
harness: harness.c
	gcc -O2 -o harness harness.c -lsimavr
firmware:
	../../scripts/compilar_firmware.sh ../firmware/blink_5s_2e/blink_5s_2e.ino blink_5s_2e.elf
medir: harness firmware
	FIRMWARE=blink_5s_2e.elf python3 medir.py 1 60 recursos_rt.csv
	FIRMWARE=blink_5s_2e.elf python3 medir.py 0 300 recursos_max.csv

// =============================================================================
// Banco de pruebas automatizado (simavr) - Entrega 2: enlace Bluetooth HC-05
// Autor: Gabriel Andres Castillo Garcia
//
// Ejecuta el MISMO firmware (ELF) del Arduino UNO en un ATmega328P emulado a
// 16 MHz y lo conecta a un modelo del HC-05 y a un "celular" programado:
//   * Modelo HC-05: modo AT cuando EN=1, modo datos transparente cuando EN=0,
//     pin STATE manejado por el banco, bytes entregados al ritmo de la línea
//     serie (10 bits por byte a la velocidad configurada).
//   * Oráculo: modelo de referencia del protocolo que predice la respuesta y el
//     estado de los LED; cada comando se verifica contra el estado real de los
//     registros del microcontrolador (PORTD, TCCR0A, OCR0A, OCR0B).
// Modos:
//   func  N seed gap_max_ms   -> N comandos aleatorios uno a uno (espera respuesta)
//   burst K                   -> K comandos seguidos sin esperar respuesta
//   failsafe                  -> encender LED y cortar el enlace (STATE=0)
// Salidas: CSV por comando en el archivo indicado y resumen en stderr.
// =============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/sim_time.h>
#include <simavr/sim_cycle_timers.h>
#include <simavr/avr_uart.h>
#include <simavr/avr_ioport.h>

#define PORTD_ADDR 0x2B
#define TCCR0A_ADDR 0x44
#define OCR0A_ADDR 0x47   // D6 (LED rojo)
#define OCR0B_ADDR 0x48   // D5 (LED verde)

static avr_t *avr;
static avr_irq_t *uart_in, *pin_state;
static long baud = 9600;
static avr_cycle_count_t byte_cycles;
static FILE *fcsv;

// ---------------- cola celular -> MCU (ritmo de línea) ----------------
static char txq[65536]; static int tq_h = 0, tq_t = 0;
static avr_cycle_count_t last_cmd_end = 0;   // ciclo en que se inyectó el '\n' del último comando
static int pending_nl = 0;                   // nº de '\n' pendientes de inyectar
static avr_cycle_count_t nl_cycle[4096]; static int nl_h = 0, nl_t = 0;

static int phone_active_flag = 0;
static avr_cycle_count_t phone_tick(avr_t *a, avr_cycle_count_t when, void *p) {
  (void)p;
  if (tq_t != tq_h) {
    uint8_t b = (uint8_t)txq[tq_t]; tq_t = (tq_t + 1) % (int)sizeof(txq);
    avr_raise_irq(uart_in, b);
    if (b == '\n') { nl_cycle[nl_h] = a->cycle + byte_cycles; nl_h = (nl_h + 1) % 4096; }
    return when + byte_cycles;
  }
  phone_active_flag = 0;
  return 0; // detener hasta nuevo envío
}
static void phone_send(const char *s) {
  for (; *s; s++) { txq[tq_h] = *s; tq_h = (tq_h + 1) % (int)sizeof(txq); }
  if (!phone_active_flag) { phone_active_flag = 1; avr_cycle_timer_register(avr, 1, phone_tick, NULL); }
}

// ---------------- recepción MCU -> HC-05 ----------------
static char at_line[128]; static int at_len = 0;
static char rx_line[256]; static int rx_len = 0;
static char lines[8192][96]; static avr_cycle_count_t line_cyc[8192]; static int n_lines = 0;
static int en_pin = 0, linked = 1;
static char bt_name[40] = "HC-05";
static avr_cycle_count_t at_first = 0, at_last = 0; static int at_count = 0;

static void mcu_to_module(const char *s) { // respuesta del módulo hacia el MCU en modo AT
  for (; *s; s++) { txq[tq_h] = *s; tq_h = (tq_h + 1) % (int)sizeof(txq); }
  if (!phone_active_flag) { phone_active_flag = 1; avr_cycle_timer_register(avr, 1, phone_tick, NULL); }
}
static void at_process(char *cmd) {
  for (char *p = cmd; *p && *p != '='; p++) *p = (char)toupper((unsigned char)*p);
  at_count++; if (!at_first) at_first = avr->cycle; at_last = avr->cycle;
  char tmp[96];
  if (!strcmp(cmd, "AT")) mcu_to_module("OK\r\n");
  else if (!strcmp(cmd, "AT+VERSION?")) mcu_to_module("+VERSION:2.0-20100601\r\nOK\r\n");
  else if (!strcmp(cmd, "AT+NAME?")) { snprintf(tmp, sizeof tmp, "+NAME:%s\r\nOK\r\n", bt_name); mcu_to_module(tmp); }
  else if (!strncmp(cmd, "AT+NAME=", 8)) { strncpy(bt_name, cmd + 8, 32); mcu_to_module("OK\r\n"); }
  else mcu_to_module("ERROR:(0)\r\n");
}
static void uart_out_cb(struct avr_irq_t *irq, uint32_t v, void *p) {
  (void)irq; (void)p; char c = (char)v;
  if (en_pin) {
    if (c == '\r') return;
    if (c == '\n') { at_line[at_len] = 0; if (at_len) at_process(at_line); at_len = 0; return; }
    if (at_len < 127) at_line[at_len++] = c;
    return;
  }
  if (!linked) return;
  if (c == '\r') return;
  if (c == '\n') {
    rx_line[rx_len] = 0;
    if (n_lines < 8192) { strncpy(lines[n_lines], rx_line, 95); lines[n_lines][95] = 0; line_cyc[n_lines] = avr->cycle; n_lines++; }
    rx_len = 0; return;
  }
  if (rx_len < 255) rx_line[rx_len++] = c;
}
static void en_cb(struct avr_irq_t *irq, uint32_t v, void *p) { (void)irq; (void)p; en_pin = v ? 1 : 0; }

// cambios de pin de los LED (para latencia comando -> LED)
static avr_cycle_count_t last_pin_change = 0;
static void led_cb(struct avr_irq_t *irq, uint32_t v, void *p) { (void)irq; (void)v; (void)p; if (!last_pin_change) last_pin_change = avr->cycle; } // primer cambio tras el envío

// ---------------- oráculo del protocolo ----------------
static int refV = 0, refR = 0;
typedef struct { char resp[96]; int kind; } expect_t; // kind 0=OK exacto 1=prefijo
static expect_t oracle(const char *cmd_in) {
  expect_t e; e.kind = 0;
  char cmd[256]; int n = 0;
  for (const char *p = cmd_in; *p && n < 255; p++) if (*p != '\r' && *p != '\n') cmd[n++] = (char)toupper((unsigned char)*p);
  cmd[n] = 0;
  if (n > 31) { strcpy(e.resp, "ERR linea demasiado larga"); return e; }
  #define OKR() snprintf(e.resp, sizeof e.resp, "OK V=%d R=%d", refV, refR)
  int val; char extra;
  if (!strcmp(cmd, "V1") || !strcmp(cmd, "1")) { refV = 255; OKR(); }
  else if (!strcmp(cmd, "V0") || !strcmp(cmd, "2")) { refV = 0; OKR(); }
  else if (!strcmp(cmd, "R1") || !strcmp(cmd, "3")) { refR = 255; OKR(); }
  else if (!strcmp(cmd, "R0") || !strcmp(cmd, "4")) { refR = 0; OKR(); }
  else if (!strcmp(cmd, "ALL1")) { refV = refR = 255; OKR(); }
  else if (!strcmp(cmd, "ALL0")) { refV = refR = 0; OKR(); }
  else if (!strncmp(cmd, "V=", 2) || !strncmp(cmd, "R=", 2)) {
    const char *s = cmd + 2; int ok = *s != 0; long acc = 0;
    for (; *s && ok; s++) { if (*s < '0' || *s > '9') ok = 0; else { acc = acc * 10 + (*s - '0'); if (acc > 255) ok = 0; } }
    if (ok) { if (cmd[0] == 'V') refV = (int)acc; else refR = (int)acc; OKR(); }
    else strcpy(e.resp, "ERR valor fuera de 0..255");
    (void)val; (void)extra;
  }
  else if (!strcmp(cmd, "STATUS")) { snprintf(e.resp, sizeof e.resp, "STATUS V=%d R=%d BT=", refV, refR); e.kind = 1; }
  else if (!strcmp(cmd, "PING")) strcpy(e.resp, "PONG");
  else if (!strcmp(cmd, "HELP")) { strcpy(e.resp, "Comandos:"); e.kind = 1; }
  else strcpy(e.resp, "ERR comando desconocido");
  return e;
}
// Verifica el estado eléctrico real de un LED contra el brillo esperado
static int pin_ok(int bright, int bit, uint8_t ocr_addr, uint8_t com_mask) {
  uint8_t portd = avr->data[PORTD_ADDR], tccr = avr->data[TCCR0A_ADDR];
  if (bright == 0)   return !(portd & (1 << bit)) && !(tccr & com_mask);
  if (bright == 255) return  (portd & (1 << bit)) && !(tccr & com_mask);
  return (tccr & com_mask) && avr->data[ocr_addr] == bright;
}
static int leds_ok(void) {
  return pin_ok(refV, 5, OCR0B_ADDR, 0x20) && pin_ok(refR, 6, OCR0A_ADDR, 0x80);
}

// ---------------- generador de comandos ----------------
static const char *VALID[] = { "V1","V0","R1","R0","ALL1","ALL0","STATUS","PING","1","2","3","4","HELP" };
static void gen_cmd(char *out) {
  int r = rand() % 100;
  if (r < 55) { strcpy(out, VALID[rand() % 13]); }
  else if (r < 75) { sprintf(out, "%c=%d", rand() % 2 ? 'V' : 'R', rand() % 256); }
  else if (r < 80) { sprintf(out, "%c=%d", rand() % 2 ? 'V' : 'R', 256 + rand() % 800); }   // fuera de rango
  else if (r < 85) { strcpy(out, VALID[rand() % 6]); for (char *p = out; *p; p++) *p = (char)tolower(*p); } // minúsculas
  else if (r < 92) { int n = 2 + rand() % 6; for (int i = 0; i < n; i++) out[i] = (char)('A' + rand() % 26); out[n] = 0; } // basura
  else if (r < 96) { strcpy(out, "V=1a"); }
  else { int n = 32 + rand() % 20; for (int i = 0; i < n; i++) out[i] = 'X'; out[n] = 0; } // línea larga
}

// ---------------- ejecución ----------------
static double cyc_ms(avr_cycle_count_t c) { return c * 1000.0 / avr->frequency; }
static int run_until(avr_cycle_count_t limit, int (*cond)(void)) {
  while (avr->cycle < limit) {
    int st = avr_run(avr);
    if (st == cpu_Done || st == cpu_Crashed) return -1;
    if (cond && cond()) return 1;
  }
  return 0;
}
static int want_lines = 0;
static int have_lines(void) { return n_lines >= want_lines; }

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "uso: %s firmware.elf baud salida.csv modo [args]\n", argv[0]); return 1; }
  const char *elf = argv[1]; baud = atol(argv[2]); fcsv = fopen(argv[3], "w"); const char *mode = argv[4];
  elf_firmware_t fw; memset(&fw, 0, sizeof fw); elf_read_firmware(elf, &fw);
  avr = avr_make_mcu_by_name("atmega328p"); avr_init(avr); avr->frequency = 16000000; avr_load_firmware(avr, &fw);
  byte_cycles = (avr_cycle_count_t)(10.0 * avr->frequency / baud + 0.5);
  uint32_t f = 0; avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS('0'), &f); f &= ~AVR_UART_FLAG_STDIO; avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS('0'), &f);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT), uart_out_cb, NULL);
  uart_in = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_INPUT);
  pin_state = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 2);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 7), en_cb, NULL);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 5), led_cb, NULL);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 6), led_cb, NULL);
  avr_raise_irq(pin_state, 1);       // celular ya emparejado
  struct timespec w0, w1; clock_gettime(CLOCK_MONOTONIC, &w0);

  // arranque: configuración AT + saludo (2 líneas)
  want_lines = 2;
  if (run_until(avr->cycle + 3 * avr->frequency, have_lines) != 1) { fprintf(stderr, "sin saludo\n"); return 2; }
  fprintf(stderr, "arranque: AT=%d cmds entre %.2f y %.2f ms, saludo a %.2f ms, nombre=%s\n",
          at_count, cyc_ms(at_first), cyc_ms(at_last), cyc_ms(line_cyc[1]), bt_name);
  int base = n_lines;

  if (!strcmp(mode, "func")) {
    int N = atoi(argv[5]); srand(atoi(argv[6])); int gapmax = atoi(argv[7]);
    fprintf(fcsv, "i,cmd,esperado,recibido,resp_ok,leds_ok,t_cmd_ms,lat_resp_ms,lat_led_ms\n");
    int pass = 0, fail = 0, leds_fail = 0;
    for (int i = 0; i < N; i++) {
      char cmd[64]; gen_cmd(cmd);
      int saveV = refV, saveR = refR;
      expect_t e = oracle(cmd);
      int digital = (saveV == 0 || saveV == 255) && (saveR == 0 || saveR == 255) && (refV == 0 || refV == 255) && (refR == 0 || refR == 255);
      int changes = digital && (refV != saveV || refR != saveR);   // latencia al LED solo sin PWM activo
      char line[80]; snprintf(line, sizeof line, "%s\n", cmd);
      last_pin_change = 0;
      phone_send(line);
      want_lines = n_lines + 1;
      if (run_until(avr->cycle + avr->frequency, have_lines) != 1) { fprintf(stderr, "timeout en %d\n", i); break; }
      avr_cycle_count_t tcmd = nl_cycle[(nl_h + 4095) % 4096];
      const char *got = lines[n_lines - 1];
      int rok = e.kind == 0 ? !strcmp(got, e.resp) : !strncmp(got, e.resp, strlen(e.resp));
      int lok = leds_ok();
      // HELP y STATUS responden en una línea; se consumen igual
      double lat_led = (changes && last_pin_change >= tcmd) ? cyc_ms(last_pin_change - tcmd) : -1;
      fprintf(fcsv, "%d,\"%s\",\"%s\",\"%s\",%d,%d,%.4f,%.4f,%.4f\n", i, cmd, e.resp, got, rok, lok,
              cyc_ms(tcmd), cyc_ms(line_cyc[n_lines - 1] - tcmd), lat_led);
      if (rok && lok) pass++; else { fail++; if (!lok) leds_fail++; }
      int gap = gapmax ? rand() % (gapmax + 1) : 0;
      run_until(avr->cycle + (avr_cycle_count_t)gap * avr->frequency / 1000, NULL);
    }
    fprintf(stderr, "func baud=%ld N=%d pass=%d fail=%d leds_fail=%d\n", baud, N, pass, fail, leds_fail);
  }
  else if (!strcmp(mode, "burst")) {
    int K = atoi(argv[5]); srand(K);
    fprintf(fcsv, "i,cmd,esperado\n");
    char exp_list[4096][96];
    for (int i = 0; i < K; i++) {
      const char *cmd = VALID[rand() % 6];        // solo comandos de LED (respuesta OK)
      expect_t e = oracle(cmd); strcpy(exp_list[i], e.resp);
      char line[16]; snprintf(line, sizeof line, "%s\n", cmd); phone_send(line);
      fprintf(fcsv, "%d,%s,\"%s\"\n", i, cmd, e.resp);
    }
    avr_cycle_count_t t0 = avr->cycle;
    want_lines = base + K;
    run_until(avr->cycle + (avr_cycle_count_t)(K * 40.0 * byte_cycles + 2 * avr->frequency), have_lines);
    int got = n_lines - base, match = 0;
    for (int i = 0, j = base; i < K && j < n_lines; i++) if (!strcmp(lines[j], exp_list[i])) { match++; j++; }
    double dur = cyc_ms((n_lines > base ? line_cyc[n_lines - 1] : avr->cycle) - t0);
    fprintf(stderr, "burst baud=%ld K=%d respuestas=%d coinciden_en_orden=%d perdidas=%d dur_ms=%.2f leds_ok=%d\n",
            baud, K, got, match, K - got, dur, leds_ok());
    printf("%ld,%d,%d,%d,%d,%.3f,%d\n", baud, K, got, match, K - got, dur, leds_ok());
  }
  else if (!strcmp(mode, "failsafe")) {
    phone_send("ALL1\n"); want_lines = n_lines + 1; run_until(avr->cycle + avr->frequency, have_lines);
    int before = (avr->data[PORTD_ADDR] >> 5) & 3;
    avr_cycle_count_t t0 = avr->cycle; last_pin_change = 0;
    linked = 0; avr_raise_irq(pin_state, 0);
    run_until(avr->cycle + avr->frequency / 10, NULL);
    int after = (avr->data[PORTD_ADDR] >> 5) & 3;
    fprintf(stderr, "failsafe: LED antes=%d despues=%d t_apagado=%.4f ms\n", before, after, cyc_ms(last_pin_change - t0));
    printf("%d,%d,%.4f\n", before, after, cyc_ms(last_pin_change - t0));
  }
  clock_gettime(CLOCK_MONOTONIC, &w1);
  double wall = (w1.tv_sec - w0.tv_sec) + (w1.tv_nsec - w0.tv_nsec) * 1e-9;
  fprintf(stderr, "tiempo_simulado=%.3f s tiempo_real=%.3f s factor=%.2f\n", cyc_ms(avr->cycle) / 1000, wall, cyc_ms(avr->cycle) / 1000 / wall);
  fclose(fcsv);
  return 0;
}

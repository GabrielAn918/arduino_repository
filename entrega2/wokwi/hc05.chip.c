// =============================================================================
// Modelo de comportamiento del módulo Bluetooth HC-05 para Wokwi (Chips API).
// Entrega 2 - Internet de las cosas - Politécnico Grancolombiano
// Autor: Gabriel Andres Castillo Garcia
//
// Qué modela:
//   * Lado MCU: UART TXD/RXD a 9600 baudios 8N1 (configuración de fábrica).
//   * Lado "aire": UART LINK_TX/LINK_RX conectada al monitor serie de Wokwi,
//     que hace las veces de la app de terminal Bluetooth del celular (perfil SPP).
//   * Pin STATE: ALTO cuando hay enlace emparejado (control "Enlace" del chip).
//   * Pin EN (KEY): ALTO = modo de comandos AT; BAJO = modo de datos transparente.
//   * Subconjunto de comandos AT del firmware HC-05 v2.0.
//   * Modo de autoprueba (atributo autotest): el chip actúa como un celular que
//     envía comandos y mide la latencia de ida y vuelta en tiempo simulado.
// Qué NO modela: la capa de radio (RF, BR/EDR, saltos de frecuencia, pérdidas).
// =============================================================================
#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define QSIZE 1024

typedef struct {
  uart_dev_t dev;
  uint8_t q[QSIZE];
  uint8_t qsrc[QSIZE];        // 1 = byte de un comando del celular (para medir latencia)
  int head, tail;
  bool busy;
  uint8_t cur;
  uint8_t cur_src;
} txq_t;

typedef struct {
  pin_t state_pin, en_pin;
  txq_t mcu, air;
  uint32_t attr_link, attr_autotest, attr_period;
  timer_t poll_timer, auto_timer;
  int linked;
  char name[33];
  char at_buf[80]; int at_len;
  char rsp_buf[160]; int rsp_len;
  char cmd_buf[80]; int cmd_len;
  // medición de latencia
  uint64_t t_cmd_end;
  bool waiting;
  uint32_t n_cmd, n_rsp, n_drop;
  double lat_sum, lat_min, lat_max;
  // autoprueba
  int auto_total, auto_i;
  bool auto_started, auto_done;
} chip_t;

static double now_ms(void) { return (double)get_sim_nanos() / 1e6; }

// ---------- colas de transmisión (un byte por llamada a uart_write) ----------
static void q_kick(txq_t *q) {
  if (q->busy || q->head == q->tail) return;
  q->cur = q->q[q->tail];
  q->cur_src = q->qsrc[q->tail];
  q->tail = (q->tail + 1) % QSIZE;
  q->busy = true;
  if (!uart_write(q->dev, &q->cur, 1)) q->busy = false;
}
static void q_push(txq_t *q, const char *s, int n, uint8_t src) {
  for (int i = 0; i < n; i++) {
    int nh = (q->head + 1) % QSIZE;
    if (nh == q->tail) break;              // cola llena: se descarta
    q->q[q->head] = (uint8_t)s[i];
    q->qsrc[q->head] = src;
    q->head = nh;
  }
  q_kick(q);
}
static void q_str(txq_t *q, const char *s) { q_push(q, s, (int)strlen(s), 0); }

// ---------- intérprete de comandos AT ----------
static void at_reply(chip_t *c, const char *line) {
  q_str(&c->mcu, line);
  q_str(&c->mcu, "\r\n");
}
static void at_process(chip_t *c, char *cmd) {
  for (char *p = cmd; *p && *p != '='; p++) *p = (char)toupper((unsigned char)*p);
  char tmp[80];
  printf("[%.3f ms] AT <- %s\n", now_ms(), cmd);
  if (strcmp(cmd, "AT") == 0) { at_reply(c, "OK"); return; }
  if (strcmp(cmd, "AT+VERSION?") == 0) { at_reply(c, "+VERSION:2.0-20100601"); at_reply(c, "OK"); return; }
  if (strcmp(cmd, "AT+NAME?") == 0) { snprintf(tmp, sizeof tmp, "+NAME:%s", c->name); at_reply(c, tmp); at_reply(c, "OK"); return; }
  if (strncmp(cmd, "AT+NAME=", 8) == 0) { strncpy(c->name, cmd + 8, 32); c->name[32] = 0; at_reply(c, "OK"); return; }
  if (strcmp(cmd, "AT+ADDR?") == 0) { at_reply(c, "+ADDR:98d3:31:f5a1b2"); at_reply(c, "OK"); return; }
  if (strcmp(cmd, "AT+ROLE?") == 0) { at_reply(c, "+ROLE:0"); at_reply(c, "OK"); return; }
  if (strcmp(cmd, "AT+UART?") == 0) { at_reply(c, "+UART:9600,0,0"); at_reply(c, "OK"); return; }
  if (strcmp(cmd, "AT+PSWD?") == 0) { at_reply(c, "+PSWD:1234"); at_reply(c, "OK"); return; }
  if (strcmp(cmd, "AT+STATE?") == 0) { at_reply(c, c->linked ? "+STATE:CONNECTED" : "+STATE:PAIRABLE"); at_reply(c, "OK"); return; }
  if (strcmp(cmd, "AT+RESET") == 0) { at_reply(c, "OK"); return; }
  at_reply(c, "ERROR:(0)");
}

// ---------- callbacks UART ----------
// Byte enviado por el Arduino (pin TX del UNO -> RXD del HC-05)
static void on_mcu_rx(void *ud, uint8_t b) {
  chip_t *c = (chip_t *)ud;
  if (pin_read(c->en_pin)) {                      // modo AT
    if (b == '\r') return;
    if (b == '\n') { c->at_buf[c->at_len] = 0; if (c->at_len) at_process(c, c->at_buf); c->at_len = 0; return; }
    if (c->at_len < (int)sizeof(c->at_buf) - 1) c->at_buf[c->at_len++] = (char)b;
    return;
  }
  if (!c->linked) { c->n_drop++; return; }        // sin enlace: el módulo no tiene a quién enviar
  char ch = (char)b;
  q_push(&c->air, &ch, 1, 0);                     // transparente hacia el celular
  if (b == '\r') return;
  if (b == '\n') {
    c->rsp_buf[c->rsp_len] = 0;
    if (c->waiting && c->rsp_len > 0) {
      double lat = now_ms() - (double)c->t_cmd_end / 1e6;
      c->waiting = false; c->n_rsp++;
      c->lat_sum += lat;
      if (lat < c->lat_min) c->lat_min = lat;
      if (lat > c->lat_max) c->lat_max = lat;
      printf("[%.3f ms] RSP \"%s\" latencia=%.3f ms\n", now_ms(), c->rsp_buf, lat);
    }
    c->rsp_len = 0;
    return;
  }
  if (c->rsp_len < (int)sizeof(c->rsp_buf) - 1) c->rsp_buf[c->rsp_len++] = ch;
}
static void on_mcu_done(void *ud) {
  chip_t *c = (chip_t *)ud;
  c->mcu.busy = false;
  if (c->mcu.cur_src == 1 && c->mcu.cur == '\n') { // terminó de entregarse un comando completo
    c->t_cmd_end = get_sim_nanos();
    c->waiting = true;
  }
  q_kick(&c->mcu);
}
// Byte escrito por el usuario en el monitor serie (= app del celular)
static void on_air_rx(void *ud, uint8_t b) {
  chip_t *c = (chip_t *)ud;
  if (!c->linked) { c->n_drop++; return; }
  char ch = (char)b;
  if (b == '\n') {
    c->cmd_buf[c->cmd_len] = 0;
    c->n_cmd++;
    printf("[%.3f ms] CMD \"%s\"\n", now_ms(), c->cmd_buf);
    c->cmd_len = 0;
  } else if (b != '\r' && c->cmd_len < (int)sizeof(c->cmd_buf) - 1) c->cmd_buf[c->cmd_len++] = ch;
  q_push(&c->mcu, &ch, 1, 1);
}
static void on_air_done(void *ud) {
  chip_t *c = (chip_t *)ud;
  c->air.busy = false;
  q_kick(&c->air);
}

// ---------- autoprueba: el chip actúa como celular ----------
static const char *AUTO_CMDS[] = { "V1", "R1", "V=128", "R=64", "STATUS", "PING", "V0", "R0", "XYZ", "1", "3", "2", "4" };
#define N_AUTO (int)(sizeof(AUTO_CMDS) / sizeof(AUTO_CMDS[0]))

static void auto_tick(void *ud) {
  chip_t *c = (chip_t *)ud;
  if (!c->linked) return;
  if (c->auto_done) return;
  if (c->auto_i >= c->auto_total) {
    c->auto_done = true;
    timer_stop(c->auto_timer);
    printf("[%.3f ms] RESUMEN autoprueba: enviados=%u respuestas=%u lat_media=%.3f ms lat_min=%.3f ms lat_max=%.3f ms descartados=%u\n",
           now_ms(), c->n_cmd, c->n_rsp, c->n_rsp ? c->lat_sum / c->n_rsp : 0.0, c->lat_min, c->lat_max, c->n_drop);
    return;
  }
  const char *cmd = AUTO_CMDS[c->auto_i % N_AUTO];
  c->auto_i++;
  c->n_cmd++;
  printf("[%.3f ms] CMD \"%s\" (auto %d/%d)\n", now_ms(), cmd, c->auto_i, c->auto_total);
  char line[40];
  snprintf(line, sizeof line, "> %s\r\n", cmd);
  q_str(&c->air, line);                          // eco en la "pantalla del celular"
  q_push(&c->mcu, cmd, (int)strlen(cmd), 1);
  q_push(&c->mcu, "\n", 1, 1);
}

// ---------- sondeo del control "Enlace" ----------
static void poll_tick(void *ud) {
  chip_t *c = (chip_t *)ud;
  if (get_sim_nanos() < 500000000ULL) return;    // el módulo arranca ~0,5 s antes de aceptar el emparejamiento
  int want = attr_read(c->attr_link) ? 1 : 0;
  if (want != c->linked) {
    c->linked = want;
    pin_write(c->state_pin, want ? HIGH : LOW);
    printf("[%.3f ms] ENLACE %s\n", now_ms(), want ? "CONECTADO" : "DESCONECTADO");
    char msg[64];
    snprintf(msg, sizeof msg, want ? "\r\n[App] Conectado a %s\r\n" : "\r\n[App] Conexion perdida con %s\r\n", c->name);
    // el aviso lo genera la app del celular, no el Arduino
    q_str(&c->air, msg);
    if (want && c->auto_total > 0 && !c->auto_started) {
      c->auto_started = true;
      timer_start(c->auto_timer, attr_read(c->attr_period) * 1000, true);
    }
  }
}

void chip_init(void) {
  chip_t *c = calloc(1, sizeof(chip_t));
  strcpy(c->name, "HC-05");
  c->lat_min = 1e9;
  c->state_pin = pin_init("STATE", OUTPUT_LOW);
  c->en_pin = pin_init("EN", INPUT_PULLDOWN);
  pin_init("VCC", INPUT);
  pin_init("GND", INPUT);
  c->attr_link = attr_init("link", 1);            // 1 = el celular ya está emparejado al iniciar
  c->attr_autotest = attr_init("autotest", 0);    // nº de comandos automáticos (0 = interactivo)
  c->attr_period = attr_init("autoPeriodMs", 250);
  c->auto_total = (int)attr_read(c->attr_autotest);

  const uart_config_t mcu_cfg = {
    .tx = pin_init("TXD", INPUT_PULLUP), .rx = pin_init("RXD", INPUT),
    .baud_rate = 9600, .rx_data = on_mcu_rx, .write_done = on_mcu_done, .user_data = c,
  };
  c->mcu.dev = uart_init(&mcu_cfg);
  const uart_config_t air_cfg = {
    .tx = pin_init("LINK_TX", INPUT_PULLUP), .rx = pin_init("LINK_RX", INPUT),
    .baud_rate = 9600, .rx_data = on_air_rx, .write_done = on_air_done, .user_data = c,
  };
  c->air.dev = uart_init(&air_cfg);

  const timer_config_t poll_cfg = { .callback = poll_tick, .user_data = c };
  c->poll_timer = timer_init(&poll_cfg);
  timer_start(c->poll_timer, 10000, true);         // cada 10 ms
  const timer_config_t auto_cfg = { .callback = auto_tick, .user_data = c };
  c->auto_timer = timer_init(&auto_cfg);
  printf("HC-05 (modelo) listo. autotest=%d\n", c->auto_total);
}

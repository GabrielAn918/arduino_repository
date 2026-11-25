/*
  =============================================================================
  Entrega 2 - Control de LED por Bluetooth (Arduino UNO + HC-05)
  Internet de las cosas - Politécnico Grancolombiano
  Autor: Gabriel Andres Castillo Garcia
  =============================================================================
  Arquitectura
    Celular (app terminal SPP) ))) HC-05 <--UART 9600 8N1--> Arduino UNO --> 2 LED
  Conexiones
    HC-05 TXD   -> D0 (RX del UNO)
    HC-05 RXD   <- D1 (TX del UNO)   [en hardware real: divisor 1 kΩ / 2 kΩ a 3,3 V]
    HC-05 STATE -> D2 (ALTO = enlace emparejado)
    HC-05 EN    <- D7 (ALTO = modo de comandos AT)
    LED verde   <- D5 (PWM) + 220 Ω
    LED rojo    <- D6 (PWM) + 220 Ω
  Protocolo (líneas de texto terminadas en \n o \r\n, sin distinción de mayúsculas)
    V1 | V0         enciende / apaga el LED verde
    R1 | R0         enciende / apaga el LED rojo
    V=n | R=n       brillo PWM 0..255
    ALL1 | ALL0     ambos LED
    STATUS          estado de LED, módulo y contadores
    PING            prueba de enlace (responde PONG)
    HELP            lista de comandos
    1 2 3 4         atajos de un carácter: 1=V1 2=V0 3=R1 4=R0 (botones de apps)
  Respuestas: "OK V=<0..255> R=<0..255>" o "ERR <motivo>"
  Seguridad funcional: al perder el enlace (STATE en BAJO) ambos LED se apagan.
  Nota: el UART por hardware (D0/D1) es compartido con el USB; en una placa
  física se debe desconectar el HC-05 mientras se carga el programa.
  =============================================================================
*/

const uint8_t PIN_STATE = 2;
const uint8_t PIN_EN    = 7;
const uint8_t LED_V     = 5;
const uint8_t LED_R     = 6;
#ifndef BAUD_BT_CFG
#define BAUD_BT_CFG 9600                  // velocidad de fábrica del HC-05 (modo datos)
#endif
const long    BAUD_BT   = BAUD_BT_CFG;   // se puede cambiar al compilar (-DBAUD_BT_CFG=38400)
const char    NOMBRE_BT[] = "POLI-IOT-E2";  // nombre que se asigna al módulo

char    linea[32];                        // búfer de la línea en recepción
uint8_t largo = 0;
bool    desborde = false;
uint8_t brilloV = 0, brilloR = 0;
bool    conectado = false;
char    nombreModulo[24] = "?";
char    versionModulo[28] = "?";
unsigned long nRx = 0, nOk = 0, nErr = 0;

// ---------------------------------------------------------------------------
void aplicarLeds() {
  analogWrite(LED_V, brilloV);
  analogWrite(LED_R, brilloR);
}

void responderOk() {
  Serial.print(F("OK V=")); Serial.print(brilloV);
  Serial.print(F(" R="));   Serial.println(brilloR);
  nOk++;
}

void responderErr(const __FlashStringHelper *motivo) {
  Serial.print(F("ERR ")); Serial.println(motivo);
  nErr++;
}

// Lee una línea del módulo con tiempo límite (se usa solo en modo AT)
bool leerLineaAT(char *dst, uint8_t max, unsigned long timeoutMs) {
  uint8_t n = 0; unsigned long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') { if (n == 0) continue; dst[n] = 0; return true; }
      if (n < max - 1) dst[n++] = c;
    }
  }
  dst[n] = 0;
  return n > 0;
}

// Envía un comando AT y guarda la primera línea de respuesta en 'resp'
bool comandoAT(const char *cmd, char *resp, uint8_t max) {
  while (Serial.available()) Serial.read();
  Serial.print(cmd); Serial.print(F("\r\n"));
  if (!leerLineaAT(resp, max, 300)) return false;
  if (resp[0] == '+') {                    // respuesta con datos: falta el "OK"
    char ok[8];
    leerLineaAT(ok, sizeof ok, 300);
  }
  return strncmp(resp, "ERROR", 5) != 0;
}

// Configuración del módulo por comandos AT (EN en ALTO durante el proceso)
void configurarModulo() {
  char resp[32], cmd[40];
  digitalWrite(PIN_EN, HIGH);
  delay(20);
  comandoAT("AT", resp, sizeof resp);
  snprintf(cmd, sizeof cmd, "AT+NAME=%s", NOMBRE_BT);
  comandoAT(cmd, resp, sizeof resp);
  if (comandoAT("AT+NAME?", resp, sizeof resp) && strncmp(resp, "+NAME:", 6) == 0)
    strncpy(nombreModulo, resp + 6, sizeof nombreModulo - 1);
  if (comandoAT("AT+VERSION?", resp, sizeof resp) && strncmp(resp, "+VERSION:", 9) == 0)
    strncpy(versionModulo, resp + 9, sizeof versionModulo - 1);
  digitalWrite(PIN_EN, LOW);               // regreso a modo de datos
  delay(20);
}

void ayuda() {
  Serial.println(F("Comandos: V1 V0 R1 R0 V=n R=n ALL1 ALL0 STATUS PING HELP | 1 2 3 4"));
}

void estado() {
  Serial.print(F("STATUS V=")); Serial.print(brilloV);
  Serial.print(F(" R="));       Serial.print(brilloR);
  Serial.print(F(" BT="));      Serial.print(nombreModulo);
  Serial.print(F(" FW="));      Serial.print(versionModulo);
  Serial.print(F(" RX="));      Serial.print(nRx);
  Serial.print(F(" OK="));      Serial.print(nOk);
  Serial.print(F(" ERR="));     Serial.println(nErr);
}

// Convierte "n" (0..255) a byte; devuelve false si no es válido
bool aByte(const char *s, uint8_t *v) {
  if (!*s) return false;
  long n = 0;
  for (; *s; s++) { if (*s < '0' || *s > '9') return false; n = n * 10 + (*s - '0'); if (n > 255) return false; }
  *v = (uint8_t)n; return true;
}

void procesar(char *cmd) {
  nRx++;
  for (char *p = cmd; *p; p++) *p = toupper(*p);
  uint8_t v;
  if      (!strcmp(cmd, "V1") || !strcmp(cmd, "1")) { brilloV = 255; aplicarLeds(); responderOk(); }
  else if (!strcmp(cmd, "V0") || !strcmp(cmd, "2")) { brilloV = 0;   aplicarLeds(); responderOk(); }
  else if (!strcmp(cmd, "R1") || !strcmp(cmd, "3")) { brilloR = 255; aplicarLeds(); responderOk(); }
  else if (!strcmp(cmd, "R0") || !strcmp(cmd, "4")) { brilloR = 0;   aplicarLeds(); responderOk(); }
  else if (!strcmp(cmd, "ALL1")) { brilloV = brilloR = 255; aplicarLeds(); responderOk(); }
  else if (!strcmp(cmd, "ALL0")) { brilloV = brilloR = 0;   aplicarLeds(); responderOk(); }
  else if (!strncmp(cmd, "V=", 2)) { if (aByte(cmd + 2, &v)) { brilloV = v; aplicarLeds(); responderOk(); } else responderErr(F("valor fuera de 0..255")); }
  else if (!strncmp(cmd, "R=", 2)) { if (aByte(cmd + 2, &v)) { brilloR = v; aplicarLeds(); responderOk(); } else responderErr(F("valor fuera de 0..255")); }
  else if (!strcmp(cmd, "STATUS")) { estado(); nOk++; }
  else if (!strcmp(cmd, "PING"))   { Serial.println(F("PONG")); nOk++; }
  else if (!strcmp(cmd, "HELP"))   { ayuda(); nOk++; }
  else responderErr(F("comando desconocido"));
}

void setup() {
  pinMode(LED_V, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(PIN_EN, OUTPUT);
  pinMode(PIN_STATE, INPUT);
  aplicarLeds();
  Serial.begin(BAUD_BT);                   // enlace serie con el HC-05
  configurarModulo();
}

void loop() {
  // 1) Supervisión del enlace mediante el pin STATE
  bool st = digitalRead(PIN_STATE);
  if (st && !conectado) {
    conectado = true;
    Serial.print(F("Hola! Arduino UNO listo via ")); Serial.println(nombreModulo);
    ayuda();
  } else if (!st && conectado) {
    conectado = false;
    brilloV = brilloR = 0; aplicarLeds();  // estado seguro al perder el enlace
    largo = 0; desborde = false;
  }

  // 2) Recepción no bloqueante de comandos
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (desborde) responderErr(F("linea demasiado larga"));
      else if (largo > 0) { linea[largo] = 0; procesar(linea); }
      largo = 0; desborde = false;
    } else if (largo < sizeof(linea) - 1) {
      linea[largo++] = c;
    } else {
      desborde = true;
    }
  }
}

/*
  Uso practico de Arduino - Entrega 1
  Montaje "blink" con 5 salidas digitales (LED) y 2 entradas que simulan sensores:
   - A0: potenciometro  -> simula un sensor analogico (p. ej. nivel de luz/temperatura)
   - D2: interruptor     -> simula un sensor digital (p. ej. presencia / puerta abierta)
  El valor analogico define el periodo de parpadeo; el sensor digital cambia el modo:
   - Sensor inactivo (HIGH): secuencia de barrido (un LED a la vez)
   - Sensor activo  (LOW) : modo alarma (los 5 LED parpadean a la vez)
  Cada 250 ms se envia una linea CSV por el puerto serie para registrar las variables.
*/

const uint8_t LEDS[5] = {12, 11, 10, 9, 8};   // L1..L5
const uint8_t PIN_POT = A0;
const uint8_t PIN_SENSOR = 2;

unsigned long tPaso = 0, tLog = 0, ciclos = 0;
uint8_t indice = 0;
bool estadoAlarma = false;

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 5; i++) pinMode(LEDS[i], OUTPUT);
  pinMode(PIN_SENSOR, INPUT_PULLUP);
  Serial.println("t_ms,pot,sensor,modo,periodo_ms,L1,L2,L3,L4,L5,loops");
}

void loop() {
  ciclos++;
  int pot = analogRead(PIN_POT);                      // 0..1023
  bool sensorActivo = (digitalRead(PIN_SENSOR) == LOW);
  unsigned long periodo = map(pot, 0, 1023, 100, 1000); // 100..1000 ms
  unsigned long ahora = millis();

  if (ahora - tPaso >= periodo) {
    tPaso = ahora;
    if (sensorActivo) {                 // modo alarma
      estadoAlarma = !estadoAlarma;
      for (uint8_t i = 0; i < 5; i++) digitalWrite(LEDS[i], estadoAlarma);
    } else {                            // modo barrido
      for (uint8_t i = 0; i < 5; i++) digitalWrite(LEDS[i], i == indice);
      indice = (indice + 1) % 5;
    }
  }

  if (ahora - tLog >= 250) {
    tLog = ahora;
    Serial.print(ahora); Serial.print(',');
    Serial.print(pot); Serial.print(',');
    Serial.print(sensorActivo); Serial.print(',');
    Serial.print(sensorActivo ? "ALARMA" : "BARRIDO"); Serial.print(',');
    Serial.print(periodo);
    for (uint8_t i = 0; i < 5; i++) { Serial.print(','); Serial.print(digitalRead(LEDS[i])); }
    Serial.print(','); Serial.println(ciclos);
    ciclos = 0;
  }
}

/*
  Validación de hipótesis de la Entrega 1 (Gabriel Andres Castillo Garcia)
  H2: la diferencia de iteraciones/s entre Wokwi y simavr (-10,4 %) se debe a
      cómo cada emulador modela el tiempo de conversión del ADC.
  H1: la caída de velocidad de simavr con D2 en BAJO se debe al modelado del
      puerto de E/S con pull-up, no al programa.
  Mide con el Timer1 (sin preescaler, 1 cuenta = 62,5 ns) la duración de:
      analogRead(A0), digitalRead(2) y micros(); luego informa iteraciones/s
      de un bucle que solo lee D2.
*/
#ifndef MODO_D2
#define MODO_D2 INPUT_PULLUP
#endif

uint16_t medir(uint8_t que) {
  uint16_t t0, t1;
  noInterrupts(); t0 = TCNT1; interrupts();
  if (que == 0) (void)analogRead(A0);
  else if (que == 1) (void)digitalRead(2);
  else if (que == 2) (void)micros();          // que == 3: medición vacía
  noInterrupts(); t1 = TCNT1; interrupts();
  return t1 - t0;
}

void informe(const char *nombre, uint8_t que, uint16_t n) {
  uint32_t suma = 0; uint16_t mn = 65535, mx = 0;
  uint16_t vacio = medir(3);                 // costo de la propia medición (sin operación)
  for (uint16_t i = 0; i < n; i++) {
    uint16_t c = medir(que);
    c = (c > vacio) ? c - vacio : 0;
    suma += c; if (c < mn) mn = c; if (c > mx) mx = c;
  }
  Serial.print(nombre); Serial.print(F(",ciclos_prom=")); Serial.print((float)suma / n, 1);
  Serial.print(F(",us_prom=")); Serial.print((float)suma / n / 16.0, 2);
  Serial.print(F(",min=")); Serial.print(mn); Serial.print(F(",max=")); Serial.println(mx);
}

void setup() {
  Serial.begin(115200);
  pinMode(2, MODO_D2);
  TCCR1A = 0; TCCR1B = _BV(CS10);            // Timer1 a 16 MHz
  uint16_t primera = medir(0);               // primera conversión (25 ciclos de ADC)
  Serial.print(F("analogRead_primera,ciclos=")); Serial.println(primera);
  informe("analogRead", 0, 1000);
  informe("digitalRead", 1, 1000);
  informe("micros", 2, 1000);
}

unsigned long t0 = 0, n = 0;
void loop() {
  n++;
  (void)digitalRead(2);
  if (millis() - t0 >= 1000) {
    t0 += 1000;
    Serial.print(F("loops_por_s,")); Serial.print(n);
    Serial.print(F(",D2=")); Serial.println(digitalRead(2));
    n = 0;
  }
}

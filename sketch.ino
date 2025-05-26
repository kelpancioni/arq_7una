#include <WiFi.h>
#include <HTTPClient.h>

const int inputPin  = 15;  // conta pulsos
const int testPin   = 14;  // gera pulso de teste (1 kHz)

volatile uint32_t pulseCount = 0;
unsigned long lastMs = 0, lastUs = 0;
bool pulseState = LOW;
const unsigned long intervalUs = 500000 / 250; // 500µs toggle → 1 kHz

void IRAM_ATTR countPulse() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);
  while(!Serial);

  // interrupção na leitura
  pinMode(inputPin, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(inputPin), countPulse, RISING);

  // saída de teste
  pinMode(testPin, OUTPUT);
  digitalWrite(testPin, LOW);

  lastMs = millis();
  lastUs = micros();
}

void loop() {
  unsigned long nowUs = micros();
  // gera pulso em 14
  if (nowUs - lastUs >= intervalUs) {
    lastUs += intervalUs;
    pulseState = !pulseState;
    digitalWrite(testPin, pulseState);
  }

  unsigned long nowMs = millis();
  // a cada 1 s, calcula e reseta contador
  if (nowMs - lastMs >= 1000) {
    lastMs += 1000;
    noInterrupts();
      uint32_t cnt = pulseCount;
      pulseCount = 0;
    interrupts();
    float freq = cnt;
    Serial.printf("Freq: %.1f Hz  (pulsos=%u)\n", freq, cnt);
  }
}
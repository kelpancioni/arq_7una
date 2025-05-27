#include <WiFi.h>
#include <HTTPClient.h>

const int inputPin  = 15;  // Conta pulsos (entrada)
const int testPin   = 14;  // Gera pulso de teste (saída)
const int potPin    = 34;  // Potenciômetro (entrada analógica)

volatile uint32_t pulseCount = 0;
unsigned long lastMs = 0, lastUs = 0;
bool pulseState = LOW;
unsigned long intervalUs = 500; // Inicial, será ajustado no loop

void IRAM_ATTR countPulse() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200); // Inicializa comunicação serial

  pinMode(inputPin, INPUT_PULLDOWN); // Configura pino de entrada
  attachInterrupt(digitalPinToInterrupt(inputPin), countPulse, RISING); // Interrupção para contar pulsos

  pinMode(testPin, OUTPUT); // Configura pino de saída para gerar sinal
  digitalWrite(testPin, LOW); // Inicializa em LOW

  // Não precisa configurar potPin para analógico no ESP32
  lastMs = millis();
  lastUs = micros();
}

void loop() {
  // Leitura do potenciômetro
  int potValue = analogRead(potPin); // 0 a 4095 no ESP32
  // Mapeia para frequência desejada, ex: 100 Hz a 2 kHz
  float freq = map(potValue, 0, 4095, 100, 2000);
  intervalUs = 1000000 / (2 * freq);

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
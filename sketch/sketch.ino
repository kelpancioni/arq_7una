#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "AP_91";
const char* password = "senhaforte";
const char* n8nWebhookUrl = "http://192.168.0.222:5678/webhook/c8d09c15-aa9a-4dc1-9837-18c1d1cfa414";

const int inputPin  = 15;
const int testPin   = 14;
const int potPin    = 34;

volatile uint32_t pulseCount = 0;
unsigned long lastMs = 0;
float lastFreqSent = -1;

unsigned long lastUs = 0;
unsigned long intervalUs = 500;
bool pulseState = LOW;

// Variáveis para comunicação entre tasks
float freqToSend = -1;
SemaphoreHandle_t xFreqSemaphore;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

float stableFreq = -1;
unsigned long stableStartMs = 0;
const unsigned long stablePeriodMs = 3000;

void IRAM_ATTR countPulse() {
  pulseCount++;
}

void setupWiFi() {
  Serial.print("Conectando-se a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n WiFi conectado com sucesso!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n Falha na conexão WiFi (timeout)");
  }
}

void sendDataToN8n(float frequency) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Tentando reconectar...");
    setupWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Falha ao reconectar ao WiFi. Não será possível enviar dados.");
      return;
    }
  }

  HTTPClient http;
  http.begin(n8nWebhookUrl);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  doc["frequency"] = frequency;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  Serial.print("Enviando JSON para n8n: ");
  Serial.println(jsonPayload);

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    Serial.print("Código de Resposta HTTP: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Resposta do N8N: ");
    Serial.println(response);
  } else {
    Serial.print("Erro na Requisição HTTP: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
}

// Task para envio HTTP
void httpTask(void * parameter) {
  for (;;) {
    if (xSemaphoreTake(xFreqSemaphore, portMAX_DELAY) == pdTRUE) {
      float freq;
      // Protege acesso à variável compartilhada
      taskENTER_CRITICAL(&mux);
      freq = freqToSend;
      freqToSend = -1;
      taskEXIT_CRITICAL(&mux);

      if (freq >= 0) {
        sendDataToN8n(freq);
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Pequeno delay para evitar loop apertado
  }
}

void setup() {
  Serial.begin(115200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Iniciando...");
  display.display();
  delay(1000);

  pinMode(inputPin, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(inputPin), countPulse, RISING);

  setupWiFi();

  pinMode(testPin, OUTPUT);
  digitalWrite(testPin, LOW);

  lastMs = millis();
  lastUs = micros();

  // Cria o semáforo binário
  xFreqSemaphore = xSemaphoreCreateBinary();

  // Cria a task de envio HTTP
  xTaskCreatePinnedToCore(
    httpTask,      // Função da task
    "httpTask",    // Nome da task
    4096,          // Stack size
    NULL,          // Parametro
    1,             // Prioridade
    NULL,          // Handle
    1              // Core (1 para rodar fora do core principal)
  );
}

void loop() {
  int potValue = analogRead(potPin);
  float freq = map(potValue, 0, 4095, 100, 2000); // 100 Hz a 2000 Hz
  intervalUs = 1000000 / (2 * freq);

  unsigned long nowUs = micros();
  if (nowUs - lastUs >= intervalUs) {
    lastUs += intervalUs;
    pulseState = !pulseState;
    digitalWrite(testPin, pulseState);
  }

  unsigned long nowMs = millis();
  if (nowMs - lastMs >= 1000) {
    lastMs += 1000;
    noInterrupts();
      uint32_t cnt = pulseCount;
      pulseCount = 0;
    interrupts();
    float freqMeasured = cnt;
    Serial.printf("Freq: %.1f Hz  (pulsos=%u)\n", freqMeasured, cnt);

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0,0);
    display.printf("Freq: %.1fHz", freqMeasured);
    display.setTextSize(1);
    display.setCursor(0, 32);
    display.printf("Pulsos: %u", cnt);
    display.display();

    // Lógica de estabilidade
    if (freqMeasured == stableFreq) {
      // Frequência permaneceu igual, verifica se já passou o tempo de estabilidade
      if (millis() - stableStartMs >= stablePeriodMs) {
        if (freqMeasured != lastFreqSent) {
          // Atualiza a variável compartilhada e libera o semáforo
          taskENTER_CRITICAL(&mux);
          freqToSend = freqMeasured;
          taskEXIT_CRITICAL(&mux);
          xSemaphoreGive(xFreqSemaphore);
          lastFreqSent = freqMeasured;
        }
      }
    } else {
      // Frequência mudou, reinicia o contador de estabilidade
      stableFreq = freqMeasured;
      stableStartMs = millis();
    }
  }
}
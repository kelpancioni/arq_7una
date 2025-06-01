#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // Necessário para construir o JSON

// --- CONFIGURAÇÕES WIFI ---
const char* ssid = "Wokwi-GUEST"; // <<<<<< SUBSTITUA PELO NOME DA SUA REDE WI-FI
const char* password = ""; // <<<<<< SUBSTITUA PELA SENHA DA SUA REDE WI-FI

// --- CONFIGURAÇÕES N8N WEBHOOK ---
const char* n8nWebhookUrl = "http://192.168.0.10:5678/webhook/e9e8b9ec-734c-4152-9b4d-2687de0310ed"; // <<<<<< SUBSTITUA PELO URL DO SEU WEBHOOK DO N8N

// --- Variáveis do seu código existente ---
const int inputPin  = 15;  // Conta pulsos (entrada)
const int testPin   = 14;  // Gera pulso de teste (saída)
const int potPin    = 34;  // Potenciômetro (entrada analógica)

volatile uint32_t pulseCount = 0;
unsigned long lastMs = 0, lastUs = 0;
bool pulseState = LOW;
unsigned long intervalUs = 500; // Inicial, será ajustado no loop

// --- Funções do seu código existente ---
void IRAM_ATTR countPulse() {
  pulseCount++;
}

// --- FUNÇÃO PARA CONECTAR AO WIFI ---
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


// --- FUNÇÃO PARA ENVIAR DADOS PARA O N8N VIA HTTP POST (APENAS FREQUÊNCIA) ---
void sendDataToN8n(float frequency) { // Removido o parâmetro 'timestamp'
  if (WiFi.status() != WL_CONNECTED) {
    // Se o WiFi desconectar, tenta reconectar.
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

  // Cria o objeto JSON
  // Capacidade do JSON: Ajuste conforme o tamanho do seu JSON. 200 bytes é um bom ponto de partida.
  StaticJsonDocument<200> doc;
  doc["frequency"] = frequency; // Apenas a frequência

  String jsonPayload;
  serializeJson(doc, jsonPayload); // Converte o JSON em string

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
    Serial.println(http.errorToString(httpResponseCode)); // Detalhes do erro
  }

  http.end(); // Fecha a conexão
}

// --- SETUP ---
void setup() {
  Serial.begin(115200); // Inicializa comunicação serial para depuração

  setupWiFi();     // Conecta ao WiFi
  // timeClient.begin(); // Removido: Não precisamos mais de NTP

  pinMode(inputPin, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(inputPin), countPulse, RISING);

  pinMode(testPin, OUTPUT);
  digitalWrite(testPin, LOW);

  lastMs = millis();
  lastUs = micros();
}

// --- LOOP PRINCIPAL ---
void loop() {
  // Leitura do potenciômetro
  int potValue = analogRead(potPin);
  float freqGen = map(potValue, 0, 4095, 100, 2000); // Frequência gerada pelo testPin
  intervalUs = 1000000 / (2 * freqGen);

  unsigned long nowUs = micros();
  // Gera pulso no testPin (GPIO 14)
  if (nowUs - lastUs >= intervalUs) {
    lastUs += intervalUs;
    pulseState = !pulseState;
    digitalWrite(testPin, pulseState);
  }

  unsigned long nowMs = millis();
  // A cada 1 segundo, calcula a frequência contada e envia os dados
  if (nowMs - lastMs >= 1000) {
    lastMs += 1000;
    noInterrupts();
      uint32_t cnt = pulseCount;
      pulseCount = 0;
    interrupts();
    float freqMedida = cnt; // Frequência medida pelo inputPin

    // Removido: Não precisamos mais obter o timestamp
    // String timestampEnvio = getFormattedTimestamp();

    // Envia APENAS a frequência para o n8n
    sendDataToN8n(freqMedida); // Removido o argumento 'timestampEnvio'

    Serial.printf("Freq Medida: %7.1f Hz  (pulsos=%4u)\n", freqMedida, cnt);
    Serial.println("Dados enviados para n8n.");
    Serial.println("--------------------------------------");
  }

  // Pequeno delay para evitar loop muito rápido, embora a lógica de 1s já controle isso.
  // delay(10);
}
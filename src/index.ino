#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

// Definições do DHT22
#define DHTPIN 23          // Pino do DHT22
#define DHTTYPE DHT22      // Tipo de sensor DHT22
DHT dht(DHTPIN, DHTTYPE);  // Instancia o sensor DHT22

// Definições do MAX30102
MAX30105 particleSensor;   // Instancia o sensor MAX30102
const byte RATE_SIZE = 4;  // Tamanho do buffer de BPM
byte rates[RATE_SIZE];     // Armazena os valores de BPM
byte rateSpot = 0;         // Posição atual no buffer
long lastBeat = 0;         // Marca o tempo do último batimento
float beatsPerMinute;      // BPM atual
int beatAvg;               // Média de BPM

// Configuração do servidor web
WebServer server(80);  // Servidor na porta 80

float lastTemperature = NAN;  // Temperatura anterior
float lastHumidity = NAN;     // Umidade anterior

void setup() {
  Serial.begin(115200);  // Inicia a comunicação serial
  dht.begin();           // Inicializa o sensor DHT22

  // Inicializa o MAX30102
  Serial.println("Iniciando o MAX30102...");
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Erro ao inicializar o MAX30102. Verifique a conexão.");
    while (1)
      ;  // Se não conseguir inicializar, trava o programa
  }
  particleSensor.setup();                     // Configura o sensor MAX30102
  particleSensor.setPulseAmplitudeRed(0x0A);  // Configura a intensidade da luz vermelha
  particleSensor.setPulseAmplitudeGreen(0);   // Desliga a luz verde

  // Conecta ao WiFi
  String ssid = "SUBSTITUA PELO NOME DO SEU WIFI";     // Nome da rede WiFi
  String password = "SUBSTITUA POR SUA SENHA";  // Senha da rede WiFi

  WiFi.begin(ssid, password);  // Inicia a conexão
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);  // Aguarda a conexão
    Serial.println("Conectando ao WiFi...");
  }

  Serial.println("Conectado ao WiFi!");
  Serial.print("IP do ESP32: ");
  Serial.println(WiFi.localIP());  // Imprime o IP do ESP32

  // Configuração do servidor
  server.on("/", handleRoot);       // Define a página inicial
  server.on("/dados", handleData);  // Define a URL para obter os dados em formato JSON
  server.begin();                   // Inicia o servidor
  Serial.println("Servidor iniciado!");
}

void loop() {
  server.handleClient();  // Processa as requisições do servidor

  // Leitura dos dados do MAX30102
  long irValue = particleSensor.getIR();     // Lê o valor do sensor IR
  if (checkForBeat(irValue) == true) {       // Se houver um batimento
    long delta = millis() - lastBeat;        // Tempo desde o último batimento
    lastBeat = millis();                     // Atualiza o último batimento
    beatsPerMinute = 60 / (delta / 1000.0);  // Calcula o BPM

    // Se o BPM estiver em um intervalo válido
    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;  // Armazena o BPM
      rateSpot %= RATE_SIZE;                     // Faz o buffer circular
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++) {
        beatAvg += rates[x];  // Soma os valores do buffer
      }
      beatAvg /= RATE_SIZE;  // Calcula a média do BPM
    }
  }
}

void handleRoot() {
  // Gera a página HTML com um design moderno, fundo escuro e cores vibrantes para os cards
  String html = "<!DOCTYPE html><html lang='pt-BR'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Dados - ESP32</title>";
  html += "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/css/bootstrap.min.css' rel='stylesheet'>";
  html += "<style>";
  html += "body { font-family: 'Arial', sans-serif; background-color: #212121; color: white; padding: 20px; }";
  html += ".card { margin-top: 20px; border-radius: 10px; color: white; }";
  html += ".card-title { font-size: 30px; font-weight: bold; color: white; }";
  html += ".card-text { font-size: 20px; color: white; }";
  html += ".container { max-width: 800px; margin: 0 auto; }";
  html += ".card-temp { background-color: #f44336; }";  // Vermelho vibrante para temperatura
  html += ".card-umid { background-color: #4caf50; }";  // Verde vibrante para umidade
  html += ".card-bpm { background-color: #ff9800; }";   // Laranja vibrante para BPM
  html += ".text-center { color: white; }";
  html += "</style>";
  html += "</head><body>";

  html += "<div class='container'>";
  html += "<h1 class='text-center mb-5'>Dados dos Sensores</h1>";

  // Card de Temperatura
  html += "<div class='card card-temp'>";
  html += "<div class='card-body'>";
  html += "<h5 class='card-title'>Temperatura</h5>";
  html += "<p class='card-text'><span id='temp'>Aguardando...</span> °C</p>";
  html += "</div>";
  html += "</div>";

  // Card de Umidade
  html += "<div class='card card-umid'>";
  html += "<div class='card-body'>";
  html += "<h5 class='card-title'>Umidade</h5>";
  html += "<p class='card-text'><span id='umid'>Aguardando...</span> %</p>";
  html += "</div>";
  html += "</div>";

  // Card de Batimentos Cardíacos
  html += "<div class='card card-bpm'>";
  html += "<div class='card-body'>";
  html += "<h5 class='card-title'>Batimentos Cardíacos</h5>";
  html += "<p class='card-text'><span id='bpm'>Aguardando...</span> BPM</p>";
  html += "</div>";
  html += "</div>";

  html += "</div>";  // Fim da div.container

  html += "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/js/bootstrap.bundle.min.js'></script>";
  html += "<script>";
  html += "function atualizarDados() {";
  html += "  fetch('/dados').then(response => response.json()).then(data => {";
  html += "    document.getElementById('temp').innerHTML = data.temperatura || 'N/A';";
  html += "    document.getElementById('umid').innerHTML = data.umidade || 'N/A';";
  html += "    document.getElementById('bpm').innerHTML = data.bpm || 'N/A';";
  html += "  });";
  html += "}";
  html += "setInterval(atualizarDados, 2000);";  // Atualiza os dados a cada 2 segundos
  html += "</script>";

  html += "</body></html>";
  server.send(200, "text/html", html);  // Envia a página HTML para o cliente
}


void handleData() {
  // Lê os dados do DHT22
  float currentTemperature = dht.readTemperature();
  float currentHumidity = dht.readHumidity();

  // Cria a resposta JSON com os dados
  String jsonResponse = "{";
  jsonResponse += "\"temperatura\":";
  jsonResponse += isnan(currentTemperature) ? "\"N/A\"" : String(currentTemperature);
  jsonResponse += ",";
  jsonResponse += "\"umidade\":";
  jsonResponse += isnan(currentHumidity) ? "\"N/A\"" : String(currentHumidity);
  jsonResponse += ",";
  jsonResponse += "\"bpm\":";
  jsonResponse += (beatAvg > 0) ? String(beatAvg) : "\"N/A\"";
  jsonResponse += "}";

  server.send(200, "application/json", jsonResponse);  // Envia os dados em formato JSON
}

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";

const String BOT_TOKEN = "TU_BOT_TOKEN";
const String CHAT_ID   = "TU_CHAT_ID";

const int PIN_PIR    = 4;
const int PIN_RELE   = 5;
const int PIN_BUZZER = 12;

const unsigned long TIEMPO_LUZ_MS        = 30000;
const unsigned long COOLDOWN_TELEGRAM_MS = 60000;

unsigned long ultimaDeteccion    = 0;
unsigned long ultimaNotificacion = 0;
bool luzEncendida = false;

void maullarGato() {
  for (int freq = 300; freq < 1500; freq += 30) {
    tone(PIN_BUZZER, freq, 8);
    delay(8);
  }
  noTone(PIN_BUZZER);
  delay(200);
  for (int freq = 1500; freq > 300; freq -= 50) {
    tone(PIN_BUZZER, freq, 8);
    delay(8);
  }
  noTone(PIN_BUZZER);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_RELE, LOW);

  Serial.println("Calibrando PIR...");
  delay(30000);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK — IP: " + WiFi.localIP().toString());
  enviarTelegram("Sistema sensor pasillo iniciado.");
  Serial.println("Listo! Sistema activo.");
}

void loop() {
  unsigned long ahora = millis();
  bool hayMovimiento = digitalRead(PIN_PIR) == HIGH;

  if (hayMovimiento) {
    ultimaDeteccion = ahora;
    if (!luzEncendida) {
      digitalWrite(PIN_RELE, HIGH);
      luzEncendida = true;
      Serial.println("Movimiento — luz ON");

      if (ahora - ultimaNotificacion > COOLDOWN_TELEGRAM_MS) {
        ultimaNotificacion = ahora;
        enviarTelegram("Movimiento detectado en el pasillo.");
      }
    }
  }

  if (luzEncendida) {
    maullarGato();
  }

  if (luzEncendida && (ahora - ultimaDeteccion > TIEMPO_LUZ_MS)) {
    digitalWrite(PIN_RELE, LOW);
    luzEncendida = false;
    noTone(PIN_BUZZER);
    Serial.println("Sin movimiento — luz OFF");
  }
}

void enviarTelegram(String mensaje) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN
             + "/sendMessage?chat_id=" + CHAT_ID
             + "&text=" + mensaje;

  http.begin(client, url);
  int httpCode = http.GET();
  Serial.println("Telegram HTTP: " + String(httpCode));
  http.end();
}

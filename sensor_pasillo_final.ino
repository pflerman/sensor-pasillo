#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// ── Configuración WiFi ─────────────────────────────
const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";

// ── Telegram ────────────────────────────────────────
const String BOT_TOKEN = "TU_BOT_TOKEN";
const String CHAT_ID   = "TU_CHAT_ID";

// ── Pines ───────────────────────────────────────────
const int PIN_PIR    = 4;   // D2 (GPIO4)
const int PIN_RELE   = 5;   // D1 (GPIO5)
const int PIN_BUZZER = 12;  // D6 (GPIO12)

// ── Tiempos ─────────────────────────────────────────
const unsigned long TIEMPO_LUZ_MS        = 30000;  // 30 seg luz encendida
const unsigned long COOLDOWN_TELEGRAM_MS = 60000;  // 1 min entre notificaciones

unsigned long ultimaDeteccion    = 0;
unsigned long ultimaNotificacion = 0;
bool luzEncendida = false;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_RELE, HIGH); // relé apagado

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK — IP: " + WiFi.localIP().toString());

  Serial.println("Calibrando PIR...");
  delay(30000);
  Serial.println("Listo! Sistema activo.");
  enviarTelegram("Sistema sensor pasillo iniciado.");
}

void loop() {
  unsigned long ahora = millis();
  bool hayMovimiento = digitalRead(PIN_PIR) == HIGH;

  if (hayMovimiento) {
    ultimaDeteccion = ahora;
    if (!luzEncendida) {
      digitalWrite(PIN_RELE, LOW); // enciende luz
      luzEncendida = true;
      Serial.println("Movimiento — luz ON");

      // Dos bips suaves
      tone(PIN_BUZZER, 2000, 150);
      delay(250);
      tone(PIN_BUZZER, 2000, 150);
      delay(250);
      noTone(PIN_BUZZER);
    }

    // Notificar Telegram respetando cooldown
    if (ahora - ultimaNotificacion > COOLDOWN_TELEGRAM_MS) {
      ultimaNotificacion = ahora;
      enviarTelegram("Movimiento detectado en el pasillo.");
    }
  }

  if (luzEncendida && (ahora - ultimaDeteccion > TIEMPO_LUZ_MS)) {
    digitalWrite(PIN_RELE, HIGH); // apaga
    luzEncendida = false;
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

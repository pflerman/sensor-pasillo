#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <Dusk2Dawn.h>

// ── Credenciales ─────────────────────────────────────────────
const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";

const String BOT_TOKEN = "TU_BOT_TOKEN";
const String CHAT_ID   = "TU_CHAT_ID";

// ── Pines ────────────────────────────────────────────────────
const int PIN_PIR    = 4;   // D2
const int PIN_RELE   = 5;   // D1
const int PIN_BUZZER = 12;  // D6

// ── Tiempos ──────────────────────────────────────────────────
const unsigned long TIEMPO_LUZ_MS        = 30000;
const unsigned long COOLDOWN_TELEGRAM_MS = 60000;

// ── Ubicación (Caseros, Buenos Aires) ────────────────────────
Dusk2Dawn caseros(-34.60, -58.56, -3);  // lat, lon, UTC-3

// ── NTP ──────────────────────────────────────────────────────
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET = -3 * 3600;

// ── Estado ───────────────────────────────────────────────────
unsigned long ultimaDeteccion    = 0;
unsigned long ultimaNotificacion = 0;
bool luzEncendida   = false;
bool modoNocturno   = false;
int  diaActual      = -1;
int  contadorDiario = 0;

int minutoAmanecer = 0;
int minutoAtardecer = 0;

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

  // OTA
  ArduinoOTA.setHostname("sensor-pasillo");
  ArduinoOTA.setPassword("pablo123");
  ArduinoOTA.onStart([]() { Serial.println("OTA: inicio"); });
  ArduinoOTA.onEnd([]()   { Serial.println("OTA: fin");    });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA: %u%%\r", progress * 100 / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error [%u]\n", error);
  });
  ArduinoOTA.begin();
  Serial.println("OTA listo");

  // NTP
  configTime(GMT_OFFSET, 0, NTP_SERVER);
  Serial.print("Sincronizando NTP");
  time_t now = time(nullptr);
  while (now < 100000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println(" OK");

  calcularAmanecerAtardecer();
  enviarTelegram("Sistema sensor pasillo iniciado (OTA + modo nocturno).");
  Serial.println("Listo! Sistema activo.");
}

void loop() {
  ArduinoOTA.handle();

  unsigned long ahora = millis();
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  int minutoDelDia = t->tm_hour * 60 + t->tm_min;

  // Cambio de día → resumen + recalcular amanecer/atardecer
  if (t->tm_mday != diaActual) {
    if (diaActual != -1) {
      enviarResumenDiario();
    }
    diaActual = t->tm_mday;
    contadorDiario = 0;
    calcularAmanecerAtardecer();
  }

  // Modo nocturno: entre atardecer y amanecer
  bool eraNocturno = modoNocturno;
  modoNocturno = (minutoDelDia >= minutoAtardecer || minutoDelDia < minutoAmanecer);

  if (modoNocturno && !eraNocturno) {
    char msg[80];
    sprintf(msg, "Modo nocturno activado (atardecer %02d:%02d)",
            minutoAtardecer / 60, minutoAtardecer % 60);
    enviarTelegram(msg);
  }
  if (!modoNocturno && eraNocturno) {
    char msg[80];
    sprintf(msg, "Modo diurno activado (amanecer %02d:%02d)",
            minutoAmanecer / 60, minutoAmanecer % 60);
    enviarTelegram(msg);
  }

  bool hayMovimiento = digitalRead(PIN_PIR) == HIGH;

  if (hayMovimiento) {
    ultimaDeteccion = ahora;
    contadorDiario++;

    if (!luzEncendida) {
      if (modoNocturno) {
        digitalWrite(PIN_RELE, HIGH);
        luzEncendida = true;
        Serial.println("Movimiento nocturno — luz ON");
      } else {
        Serial.println("Movimiento diurno — luz omitida");
      }

      if (ahora - ultimaNotificacion > COOLDOWN_TELEGRAM_MS) {
        ultimaNotificacion = ahora;
        String modo = modoNocturno ? "nocturno" : "diurno";
        enviarTelegram("Movimiento detectado (" + modo + ")");
      }
    }
  }

  if (luzEncendida && modoNocturno) {
    maullarGato();
  }

  if (luzEncendida && (ahora - ultimaDeteccion > TIEMPO_LUZ_MS)) {
    digitalWrite(PIN_RELE, LOW);
    luzEncendida = false;
    noTone(PIN_BUZZER);
    Serial.println("Sin movimiento — luz OFF");
  }
}

void calcularAmanecerAtardecer() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  int dia = t->tm_mday;
  int mes = t->tm_mon + 1;
  int anio = t->tm_year + 1900;
  bool dst = false;

  minutoAmanecer  = Dusk2Dawn::sunrise(caseros, anio, mes, dia, dst);
  minutoAtardecer = Dusk2Dawn::sunset(caseros, anio, mes, dia, dst);

  char msg[120];
  sprintf(msg, "Amanecer: %02d:%02d — Atardecer: %02d:%02d",
          minutoAmanecer / 60, minutoAmanecer % 60,
          minutoAtardecer / 60, minutoAtardecer % 60);
  Serial.println(msg);
}

void enviarResumenDiario() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char fecha[16];
  sprintf(fecha, "%02d/%02d/%04d", t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);

  String msg = "Resumen del dia " + String(fecha) + ":\n";
  msg += "Activaciones: " + String(contadorDiario) + "\n";
  msg += "Amanecer: " + formatoHora(minutoAmanecer) + "\n";
  msg += "Atardecer: " + formatoHora(minutoAtardecer);
  enviarTelegram(msg);
}

String formatoHora(int minutos) {
  char buf[6];
  sprintf(buf, "%02d:%02d", minutos / 60, minutos % 60);
  return String(buf);
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

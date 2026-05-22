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

// ── Tiempos ──────────────────────────────────────────────────
const unsigned long TIEMPO_LUZ_MS        = 30000;
const unsigned long COOLDOWN_TELEGRAM_MS = 60000;
const unsigned long POLL_TELEGRAM_MS     = 5000;

// ── Ubicación (Caseros, Buenos Aires) ────────────────────────
Dusk2Dawn caseros(-34.60, -58.56, -3);  // lat, lon, UTC-3

// ── NTP ──────────────────────────────────────────────────────
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET = -3 * 3600;

// ── Estado ───────────────────────────────────────────────────
unsigned long ultimaDeteccion    = 0;
unsigned long ultimaNotificacion = 0;
unsigned long ultimoPollTelegram = 0;
bool luzEncendida   = false;
bool modoNocturno   = false;
bool modoManual     = false;
int  diaActual      = -1;
int  contadorDiario = 0;
long ultimoUpdateId = 0;
String comandoPendiente = "";
String callbackPendiente = "";

int minutoAmanecer = 0;
int minutoAtardecer = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_RELE, OUTPUT);
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
  descartarMensajesViejos();
  enviarTelegramConBotones("Sistema sensor pasillo iniciado (OTA + modo nocturno).");
  Serial.println("Listo! Sistema activo.");
}

void loop() {
  ArduinoOTA.handle();

  unsigned long ahora = millis();
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  int minutoDelDia = t->tm_hour * 60 + t->tm_min;

  // Poll Telegram cada 5 segundos
  if (ahora - ultimoPollTelegram > POLL_TELEGRAM_MS) {
    ultimoPollTelegram = ahora;
    revisarComandosTelegram();
  }

  // Procesar comando después de liberar memoria de getUpdates
  if (comandoPendiente != "") {
    if (callbackPendiente != "") {
      responderCallback(callbackPendiente);
      callbackPendiente = "";
    }
    procesarComando(comandoPendiente);
    comandoPendiente = "";
  }

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

  // En modo manual el PIR no controla el relé
  if (modoManual) return;

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

    }
  }

  if (luzEncendida && (ahora - ultimaDeteccion > TIEMPO_LUZ_MS)) {
    digitalWrite(PIN_RELE, LOW);
    luzEncendida = false;
    Serial.println("Sin movimiento — luz OFF");
  }
}

void descartarMensajesViejos() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN
             + "/getUpdates?offset=-1&limit=1";
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String response = http.getString();
    int uidIdx = response.indexOf("\"update_id\":");
    if (uidIdx != -1) {
      int uidStart = uidIdx + 12;
      int uidEnd = response.indexOf(",", uidStart);
      if (uidEnd == -1) uidEnd = response.indexOf("}", uidStart);
      ultimoUpdateId = response.substring(uidStart, uidEnd).toInt();
      Serial.println("Telegram: descartados mensajes viejos, offset=" + String(ultimoUpdateId));
    }
  }
  http.end();
}

void revisarComandosTelegram() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN
             + "/getUpdates?offset=" + String(ultimoUpdateId + 1)
             + "&timeout=0&limit=1";

  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode != 200) {
    http.end();
    return;
  }

  String response = http.getString();
  http.end();

  // Extraer update_id
  int uidIdx = response.indexOf("\"update_id\":");
  if (uidIdx == -1) return;
  int uidStart = uidIdx + 12;
  int uidEnd = response.indexOf(",", uidStart);
  if (uidEnd == -1) uidEnd = response.indexOf("}", uidStart);
  ultimoUpdateId = response.substring(uidStart, uidEnd).toInt();

  // Buscar callback_data (botones inline)
  String comando = "";
  int dataIdx = response.indexOf("\"data\":\"");
  if (dataIdx != -1) {
    int dataStart = dataIdx + 8;
    int dataEnd = response.indexOf("\"", dataStart);
    comando = response.substring(dataStart, dataEnd);

    // Guardar callback ID para responder después
    int cbIdx = response.indexOf("\"callback_query\":{\"id\":\"");
    if (cbIdx != -1) {
      int cbStart = cbIdx + 23;
      int cbEnd = response.indexOf("\"", cbStart);
      callbackPendiente = response.substring(cbStart, cbEnd);
    }
  }

  // Buscar comando de texto (/on, /off, etc.)
  if (comando == "") {
    int textIdx = response.lastIndexOf("\"text\":\"");
    if (textIdx != -1) {
      int textStart = textIdx + 8;
      int textEnd = response.indexOf("\"", textStart);
      comando = response.substring(textStart, textEnd);
      if (comando.startsWith("/")) comando = comando.substring(1);
      comando.toLowerCase();
    }
  }

  if (comando == "") return;

  comandoPendiente = comando;
}

void procesarComando(String comando) {
  if (comando == "on" || comando == "prender") {
    modoManual = true;
    digitalWrite(PIN_RELE, HIGH);
    luzEncendida = true;
    enviarTelegramConBotones("Luz ENCENDIDA (manual)");
    Serial.println("Telegram: luz ON manual");

  } else if (comando == "off" || comando == "apagar") {
    modoManual = true;
    digitalWrite(PIN_RELE, LOW);
    luzEncendida = false;
    enviarTelegramConBotones("Luz APAGADA (manual)");
    Serial.println("Telegram: luz OFF manual");

  } else if (comando == "auto" || comando == "automatico") {
    modoManual = false;
    digitalWrite(PIN_RELE, LOW);
    luzEncendida = false;
    enviarTelegramConBotones("Modo AUTOMATICO activado");
    Serial.println("Telegram: modo auto");

  } else if (comando == "status" || comando == "estado") {
    String msg = "Estado del sensor:\n";
    msg += "Modo: " + String(modoManual ? "MANUAL" : "AUTOMATICO") + "\n";
    msg += "Luz: " + String(luzEncendida ? "ENCENDIDA" : "APAGADA") + "\n";
    msg += "Horario: " + String(modoNocturno ? "nocturno" : "diurno") + "\n";
    msg += "Activaciones hoy: " + String(contadorDiario);
    enviarTelegramConBotones(msg);
  }
}

void responderCallback(String callbackId) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN
             + "/answerCallbackQuery?callback_query_id=" + callbackId;
  http.begin(client, url);
  http.GET();
  http.end();
}

void calcularAmanecerAtardecer() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  int dia = t->tm_mday;
  int mes = t->tm_mon + 1;
  int anio = t->tm_year + 1900;
  bool dst = false;

  minutoAmanecer  = caseros.sunrise(anio, mes, dia, dst);
  minutoAtardecer = caseros.sunset(anio, mes, dia, dst);

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

String urlEncode(const String& text) {
  String encoded = "";
  for (unsigned int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      char buf[4];
      sprintf(buf, "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

void enviarTelegram(String mensaje) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN
             + "/sendMessage?chat_id=" + CHAT_ID
             + "&text=" + urlEncode(mensaje);

  http.begin(client, url);
  int httpCode = http.GET();
  Serial.println("Telegram HTTP: " + String(httpCode));
  http.end();
}

void enviarTelegramConBotones(String mensaje) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String keyboard = "{\"inline_keyboard\":["
    "[{\"text\":\"Prender\",\"callback_data\":\"on\"},"
     "{\"text\":\"Apagar\",\"callback_data\":\"off\"}],"
    "[{\"text\":\"Auto\",\"callback_data\":\"auto\"},"
     "{\"text\":\"Estado\",\"callback_data\":\"status\"}]"
    "]}";

  String url = "https://api.telegram.org/bot" + BOT_TOKEN
             + "/sendMessage?chat_id=" + CHAT_ID
             + "&text=" + urlEncode(mensaje)
             + "&reply_markup=" + urlEncode(keyboard);

  http.begin(client, url);
  int httpCode = http.GET();
  Serial.println("Telegram botones HTTP: " + String(httpCode));
  http.end();
}

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include <Dusk2Dawn.h>
#include <time.h>

const char* WIFI_SSID = "TCH-6956574";
const char* WIFI_PASS = "L9WGMvnhKQpctnNjCc";

const String BOT_TOKEN = "8754563237:AAFWkx645ag60sdfMaSErDoBYV3ImSwzgVc";
const String CHAT_ID   = "8239777724";

const int PIN_PIR    = 4;
const int PIN_RELE   = 5;
const int PIN_BUZZER = 12;

int activacionesHoy = 0;
int diaAnterior = -1;

// Caseros, Buenos Aires
Dusk2Dawn caseros(-34.60, -58.56, -3);

int amanecerMinutos = 0;
int atardecerMinutos = 0;
bool notificadoAmanecer = false;
bool notificadoAtardecer = false;

// Estado de la secuencia
bool secuenciaActiva = false;

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

// Prende luz + maúlla durante el tiempo indicado
void luzConMaullido(unsigned long duracionMs) {
  digitalWrite(PIN_RELE, HIGH);
  unsigned long inicio = millis();
  while (millis() - inicio < duracionMs) {
    maullarGato();
    ArduinoOTA.handle();
  }
  noTone(PIN_BUZZER);
}

// Apaga luz y espera el tiempo indicado
void luzApagadaEspera(unsigned long duracionMs) {
  digitalWrite(PIN_RELE, LOW);
  noTone(PIN_BUZZER);
  unsigned long inicio = millis();
  while (millis() - inicio < duracionMs) {
    ArduinoOTA.handle();
    delay(50);
  }
}

// Parpadeo: prende/apaga N veces con intervalo de 1 segundo
void parpadear(int veces) {
  for (int i = 0; i < veces; i++) {
    digitalWrite(PIN_RELE, LOW);
    delay(200);
    digitalWrite(PIN_RELE, HIGH);
    delay(200);
  }
}

// Secuencia completa al detectar movimiento
void ejecutarSecuencia() {
  secuenciaActiva = true;

  // 1) Parpadeo 5 veces
  parpadear(5);

  // 2) Prendida 10 segundos
  luzConMaullido(10000);

  // 3) Parpadeo 3 veces
  parpadear(3);

  // 4) Apagar todo
  digitalWrite(PIN_RELE, LOW);
  noTone(PIN_BUZZER);

  secuenciaActiva = false;
}

String minutosAHora(int minutos) {
  int h = minutos / 60;
  int m = minutos % 60;
  String hora = (h < 10 ? "0" : "") + String(h);
  String min = (m < 10 ? "0" : "") + String(m);
  return hora + ":" + min;
}

void calcularSol(struct tm* timeinfo) {
  amanecerMinutos = caseros.sunrise(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, false);
  atardecerMinutos = caseros.sunset(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, false);
  Serial.println("Amanecer: " + minutosAHora(amanecerMinutos) + " — Atardecer: " + minutosAHora(atardecerMinutos));
}

bool esDeNoche(struct tm* timeinfo) {
  int minutosActuales = timeinfo->tm_hour * 60 + timeinfo->tm_min;
  return (minutosActuales >= atardecerMinutos || minutosActuales < amanecerMinutos);
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

  // Sincronizar hora (Argentina UTC-3)
  configTime(-3 * 3600, 0, "pool.ntp.org");
  Serial.println("Sincronizando hora...");
  time_t now = time(nullptr);
  while (now < 1000000) {
    delay(500);
    now = time(nullptr);
  }
  struct tm* timeinfo = localtime(&now);
  diaAnterior = timeinfo->tm_mday;

  // Calcular sol del día
  calcularSol(timeinfo);

  ArduinoOTA.setHostname("sensor-pasillo");
  ArduinoOTA.setPassword("pablo123");
  ArduinoOTA.onStart([]() { Serial.println("Actualizando firmware..."); });
  ArduinoOTA.onEnd([]() { Serial.println("Actualización completa!"); });
  ArduinoOTA.onError([](ota_error_t error) { Serial.println("Error OTA"); });
  ArduinoOTA.begin();

  String msg = "Sensor pasillo v5 iniciado. Amanecer: " + minutosAHora(amanecerMinutos) + " - Atardecer: " + minutosAHora(atardecerMinutos);
  enviarTelegram(msg);
  Serial.println("Listo! Sistema activo.");
}

void loop() {
  ArduinoOTA.handle();

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  int minutosActuales = timeinfo->tm_hour * 60 + timeinfo->tm_min;
  int diaActual = timeinfo->tm_mday;

  // Nuevo día: recalcular sol, enviar resumen, resetear
  if (diaActual != diaAnterior) {
    String resumen = "Resumen sensor pasillo: " + String(activacionesHoy) + " activaciones ayer.";
    enviarTelegram(resumen);
    activacionesHoy = 0;
    notificadoAmanecer = false;
    notificadoAtardecer = false;
    calcularSol(timeinfo);
    diaAnterior = diaActual;
  }

  // Notificar amanecer
  if (!notificadoAmanecer && minutosActuales >= amanecerMinutos && minutosActuales < amanecerMinutos + 2) {
    enviarTelegram("Amanecer " + minutosAHora(amanecerMinutos) + " - sensor desactivado.");
    notificadoAmanecer = true;
  }

  // Notificar atardecer
  if (!notificadoAtardecer && minutosActuales >= atardecerMinutos && minutosActuales < atardecerMinutos + 2) {
    enviarTelegram("Atardecer " + minutosAHora(atardecerMinutos) + " - sensor activado.");
    notificadoAtardecer = true;
  }

  // Solo funcionar de noche
  if (esDeNoche(timeinfo)) {
    bool hayMovimiento = digitalRead(PIN_PIR) == HIGH;

    if (hayMovimiento && !secuenciaActiva) {
      activacionesHoy++;
      Serial.println("Movimiento — secuencia (#" + String(activacionesHoy) + ")");
      ejecutarSecuencia();
    }
  } else {
    // De día: asegurar que todo esté apagado
    if (secuenciaActiva) {
      digitalWrite(PIN_RELE, LOW);
      noTone(PIN_BUZZER);
      secuenciaActiva = false;
    }
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
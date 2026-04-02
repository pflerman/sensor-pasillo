const int PIN_PIR = 4; // D2 (GPIO4)

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PIR, INPUT);
  Serial.println("Sensor PIR listo en D2...");
  delay(30000); // 30 segundos de calibración
  Serial.println("Calibración lista, arrancamos!");
}

void loop() {
  if (digitalRead(PIN_PIR) == HIGH) {
    Serial.println("¡Movimiento detectado!");
    delay(1000);
  } else {
    Serial.println("Sin movimiento");
    delay(500);
  }
}

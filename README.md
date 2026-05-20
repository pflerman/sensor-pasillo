# Sensor de Movimiento PIR con ESP8266 - Pasillo

## Objetivo

4 sensores de movimiento para distintos lugares de la casa. Cada uno prende una luz via relé, suena un buzzer suave, y manda notificación por Telegram via WiFi.

## Estado: FUNCIONANDO ✓

Sistema completo probado y operativo: PIR + Relé + Buzzer + WiFi + Telegram.

## Hardware (x4 de cada uno)

- NodeMCU ESP8266 CP2102 (Duaitek) - $6.506 c/u
- PIR HC-SR501 (HobbyTronica) - $2.791 c/u
- Módulo Relé 1 canal 3.3V optoacoplado Bestep (HobbyTronica) - $6.931 c/u
- Buzzer pasivo módulo 3.3V-5V (HobbyTronica) - $2.371 c/u
- Fuente 5V 2A micro USB Pronext (Master.Leader) - $11.199 c/u
- Caja estanca Roker 92x92x75 IP65
- Prensacables PG7 (3-6.5mm)
- Cables Dupont 120 unidades (40 M-M + 40 M-H + 40 H-H)
- Silicona neutra Fischer 100ml

## Conexiones finales confirmadas

### PIR HC-SR501

```
PIR VCC  →  Vin (5V del USB)
PIR GND  →  GND (al lado de Vin)
PIR OUT  →  D2 (GPIO4)
```

### Módulo Relé Bestep 3.3V

```
Relé VCC  →  Vin (5V) — compartido con PIR
Relé GND  →  GND
Relé IN   →  D1 (GPIO5)
Jumper    →  HIGH (de fábrica, recomendado)
```

### Buzzer Pasivo

```
Buzzer S (+)  →  D6 (GPIO12)
Buzzer medio  →  3V3
Buzzer - (-)  →  GND
```

## Lecciones aprendidas

### Voltajes (CRÍTICO)

- El PIR HC-SR501 necesita mínimo 4.5V → usar Vin (5V), NO 3V3
- El relé Bestep aunque dice 3.3V, al activarse consume un pico de corriente que reinicia el ESP8266 si se alimenta desde 3V3 → usar Vin (5V)
- El buzzer pasivo sí funciona bien con 3V3 (consume muy poco)
- Cuando dos componentes necesitan 5V, compartir el pin Vin pelando cables y empatando (o usar protoboard)

### PIR HC-SR501

- Los dos potenciómetros vienen al máximo de fábrica → girar ambos al mínimo para empezar
- Potenciómetro de SENSIBILIDAD: ajusta distancia de detección (3-7m) → ajustar según instalación
- Potenciómetro de TIEMPO: dejarlo siempre al mínimo → el tiempo se controla por código
- Necesita 30-60 segundos de calibración al encender → alejarse durante ese tiempo
- La lente Fresnel (óvalo blanco) es necesaria para el ángulo de detección amplio
- Debajo de la lente están impresos los nombres de los pines

### Lógica del Relé (Jumper)

- El módulo Bestep tiene un jumper físico para elegir HIGH o LOW trigger
- Jumper en HIGH (posición de fábrica, recomendado): `digitalWrite LOW` = relé apagado, `digitalWrite HIGH` = relé activado
- Jumper en LOW: lógica invertida, LOW activa y HIGH apaga
- Dejar el jumper como viene de fábrica (HIGH) y adaptar el código es lo más seguro
- Con jumper en HIGH, al bootear el ESP8266 el relé NO se activa accidentalmente porque los pines arrancan en LOW
- La bornera de potencia tiene 3 tornillos: COM (medio), NO (Normally Open) y NC (Normally Closed)
- Para que la luz se prenda al detectar: conectar los cables en COM y NO
- Si la luz queda prendida siempre y se apaga al detectar, los cables están en COM y NC → moverlos a NO
- Los pines de control se conectan con Dupont macho atornillado en la bornera
- Click audible confirma activación

### ESP8266 NodeMCU

- Pin D2 (GPIO4) ideal para PIR, sin conflictos de boot
- Pin D1 (GPIO5) para relé
- Pin D6 (GPIO12) para buzzer
- Chip CP2102 funciona en Fedora Linux sin drivers adicionales
- Si un pin lee HIGH constante sin cable conectado, es "pin flotante" → probar otro pin

### Buzzer pasivo

- Tiene 3 pines: S (señal), medio (VCC), - (GND)
- Se controla volumen y frecuencia por código con `tone()`
- Frecuencia más alta = sonido más agudo y perceptible
- No tiene mucha potencia, para alarma real necesitás sirena piezoeléctrica 12V

## Software

- **Arduino IDE 2.3.8** (AppImage en `~/arduino-ide.AppImage`)
- **Board:** ESP8266 Community → NodeMCU 1.0 (ESP-12E Module)
- **Puerto:** `/dev/ttyUSB0`
- **Board Manager URL:** `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
- Fedora necesita FUSE: `sudo dnf install fuse fuse-libs`
- Usuario debe estar en grupo dialout: `sudo usermod -aG dialout pepe` + `newgrp dialout`

## Bot de Telegram

- Crear bot con @BotFather → `/newbot`
- Obtener chat_id: mandar mensaje al bot y consultar `getUpdates`
- Cooldown de 60 segundos entre notificaciones para no spamear

## Parámetros ajustables en el código

- `TIEMPO_LUZ_MS`: cuánto tiempo queda encendida la luz (default 30 seg)
- `COOLDOWN_TELEGRAM_MS`: tiempo mínimo entre notificaciones Telegram (default 60 seg)
- `tone(PIN_BUZZER, frecuencia, duración)`: ajustar sonido del buzzer

## Próximos pasos

1. Instalar en caja estanca con silicona en la lente del PIR
2. Conectar luz 220V a la bornera del relé (COM y NO)
3. Replicar para los 4 sensores
4. Agregar control por horario (solo de noche) via NTP

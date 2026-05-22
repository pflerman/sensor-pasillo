# Sensor de Movimiento PIR con ESP8266 - Pasillo

## Objetivo

4 sensores de movimiento para distintos lugares de la casa. Cada uno prende una luz via relé y manda notificación por Telegram via WiFi.

## Estado: FUNCIONANDO ✓

Sistema completo probado y operativo: PIR + Relé + WiFi + Telegram + OTA + modo nocturno automático.

## Funcionalidades

- **Detección PIR** con luz (relé) — solo de noche
- **Control remoto por Telegram** con botones inline
  - **Prender** / **Apagar** — control manual de la luz
  - **Auto** — vuelve al modo automático (PIR + nocturno)
  - **Estado** — muestra modo, luz, horario y activaciones
  - Comandos de texto: `/on`, `/off`, `/auto`, `/estado`
  - Polling cada 5 segundos via getUpdates
- **OTA (Over The Air)** — actualización inalámbrica sin cable USB
  - Hostname: `sensor-pasillo`
  - Password: `pablo123`
  - Desde Arduino IDE: Puerto → `sensor-pasillo at 192.168.0.179`
  - Desde terminal: `espota.py -i 192.168.0.179 -a pablo123 -f firmware.bin`
- **Modo nocturno automático** con Dusk2Dawn
  - Coordenadas Caseros: -34.60, -58.56, UTC-3
  - La luz solo se prende de noche (entre atardecer y amanecer)
  - De día detecta movimiento pero no activa relé
  - Notificación por Telegram al cambiar de modo (amanecer/atardecer)
- **Resumen diario por Telegram** a medianoche
  - Cantidad de activaciones del día
  - Horarios de amanecer y atardecer calculados

## Hardware (x4 de cada uno)

- NodeMCU ESP8266 CP2102 (Duaitek) - $6.506 c/u
- PIR HC-SR501 (HobbyTronica) - $2.791 c/u
- Módulo Relé 1 canal 3.3V optoacoplado Bestep (HobbyTronica) - $6.931 c/u
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

## Lecciones aprendidas

### Voltajes (CRITICO)

- El PIR HC-SR501 necesita mínimo 4.5V → usar Vin (5V), NO 3V3
- El relé Bestep aunque dice 3.3V, al activarse consume un pico de corriente que reinicia el ESP8266 si se alimenta desde 3V3 → usar Vin (5V)
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
- Chip CP2102 funciona en Fedora Linux sin drivers adicionales
- Si un pin lee HIGH constante sin cable conectado, es "pin flotante" → probar otro pin

### Dusk2Dawn en Linux (bugs conocidos)

La librería Dusk2Dawn tiene bugs que rompen la compilación en Linux:

1. **`Math.h` → `math.h`**: el include usa `Math.h` (mayúscula) que solo existe en Windows/macOS. En Linux es case-sensitive → renombrar a `math.h` en el `.cpp` de la librería.
2. **`static` en `min2str` y `zeroPadTime`**: estas funciones están declaradas `static` en el `.cpp` pero expuestas en el `.h` → quitar `static` del `.cpp` para que linkee correctamente.

Ubicación típica de la librería:
```
~/Arduino/libraries/Dusk2Dawn/src/Dusk2Dawn.cpp
```

### OTA en Fedora Linux (firewall)

El upload OTA requiere que el ESP8266 se conecte de vuelta a la PC en un puerto random (10000-60000). Fedora bloquea esto por defecto.

```bash
# Abrir puertos permanente (una sola vez)
sudo firewall-cmd --add-port=10000-60000/tcp --permanent
sudo firewall-cmd --reload
```

Sin esto, `espota.py` autentica OK pero falla con "No response from device".

### Upload OTA desde terminal

Arduino IDE 2.x tiene un bug con OTA (`{upload.port.properties.port}`). Usar `arduino-cli` + `espota.py`:

```bash
# Compilar
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --output-dir /tmp/sensor-build sensor_pasillo_final/

# Subir por OTA
python3 ~/.arduino15/packages/esp8266/hardware/esp8266/3.1.2/tools/espota.py \
  -i 192.168.0.179 -a pablo123 -f /tmp/sensor-build/sensor_pasillo_final.ino.bin
```

## Software

- **Arduino IDE 2.3.8** (AppImage en `~/arduino-ide.AppImage`)
- **arduino-cli** instalado en `~/.local/bin/arduino-cli`
- **Board:** ESP8266 Community → NodeMCU 1.0 (ESP-12E Module)
- **Puerto:** `/dev/ttyUSB0` (cable) o `sensor-pasillo at 192.168.0.179` (OTA por red)
- **Board Manager URL:** `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
- **Librerías adicionales:** Dusk2Dawn (instalar desde Library Manager)
- Fedora necesita FUSE: `sudo dnf install fuse fuse-libs`
- Usuario debe estar en grupo dialout: `sudo usermod -aG dialout pepe` + `newgrp dialout`

## Bot de Telegram

- Crear bot con @BotFather → `/newbot`
- Obtener chat_id: mandar mensaje al bot y consultar `getUpdates`
- Cooldown de 60 segundos entre notificaciones para no spamear
- Resumen diario automático a medianoche con cantidad de activaciones

## Parámetros ajustables en el código

- `TIEMPO_LUZ_MS`: cuánto tiempo queda encendida la luz (default 30 seg)
- `COOLDOWN_TELEGRAM_MS`: tiempo mínimo entre notificaciones Telegram (default 60 seg)
- Coordenadas Dusk2Dawn: latitud, longitud, timezone UTC offset
- OTA hostname y password

## Próximos pasos

1. Instalar en caja estanca con silicona en la lente del PIR
2. Conectar luz 220V a la bornera del relé (COM y NO)
3. Replicar para los 4 sensores

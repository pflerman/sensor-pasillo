# Sensor de Movimiento PIR con ESP8266 - Pasillo

## Objetivo

4 sensores de movimiento para distintos lugares de la casa. Cada uno prende una luz, suena un buzzer suave, y manda notificación por Telegram.

## Hardware comprado (x4 de cada uno)

- NodeMCU ESP8266 CP2102 (Duaitek) - $6.506 c/u
- PIR HC-SR501 (HobbyTronica) - $2.791 c/u
- Módulo Relé 1 canal 3.3V optoacoplado Bestep (HobbyTronica) - $6.931 c/u
- Fuente 5V 2A micro USB Pronext (Master.Leader) - $11.199 c/u
- Caja estanca Roker 92x92x75 IP65
- Prensacables PG7 (3-6.5mm)
- Cables Dupont 120 unidades (40 M-M + 40 M-H + 40 H-H)
- Silicona neutra Fischer 100ml
- Buzzer pasivo (para futuro)

## Conexiones confirmadas y funcionando

```
PIR VCC  →  Vin (5V del USB)
PIR GND  →  GND (el de al lado de Vin)
PIR OUT  →  D2 (GPIO4)
```

## Lecciones aprendidas (debugging)

- El PIR HC-SR501 necesita mínimo 4.5V → usar Vin (5V), NO 3V3
- Los dos potenciómetros del PIR vienen al máximo de fábrica → girar ambos al mínimo para empezar
- El potenciómetro de SENSIBILIDAD ajusta distancia de detección (3-7m) → ajustar según instalación
- El potenciómetro de TIEMPO dejarlo siempre al mínimo → el tiempo se controla por código
- El PIR necesita 30-60 segundos de calibración al encender → alejarse durante ese tiempo
- La lente Fresnel (óvalo blanco) es necesaria para el ángulo de detección amplio
- Pin D2 (GPIO4) es ideal para el PIR, sin conflictos de boot
- El NodeMCU con chip CP2102 funciona en Fedora Linux sin drivers adicionales
- Arduino IDE 2.3.8 en Fedora necesita FUSE instalado (`sudo dnf install fuse fuse-libs`)
- El usuario debe estar en el grupo `dialout` para acceder al puerto USB

## Software instalado en Fedora

- **Arduino IDE 2.3.8** (AppImage en `~/arduino-ide.AppImage`)
- **Board:** ESP8266 Community → NodeMCU 1.0 (ESP-12E Module)
- **Puerto:** `/dev/ttyUSB0`
- **Board Manager URL:** `http://arduino.esp8266.com/stable/package_esp8266com_index.json`

## Próximos pasos

1. Conectar el relé 3.3V y probar encender/apagar una luz
2. Crear bot de Telegram (@BotFather)
3. Código completo: WiFi + PIR + Relé + Telegram
4. Agregar buzzer pasivo con volumen controlado por código
5. Instalar en caja estanca con silicona en la lente del PIR
6. Replicar para los 4 sensores

## Código final futuro (ESP8266 + PIR + Relé + WiFi + Telegram)

Pendiente de implementar.

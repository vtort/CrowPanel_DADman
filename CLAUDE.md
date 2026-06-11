# CrowPanel Dolby Renderer — Guía completa

Controlador de volumen master para el **Dolby Atmos Renderer** usando un **CrowPanel ESP32-S3 1.28"** (pantalla redonda GC9A01 240x240). Un CrowPanel por sala de mezcla.

## Hardware
- **Board**: CrowPanel ESP32-S3 1.28" round display
- **FQBN**: `esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi`

## Protocolo de control
OSC sobre WiFi UDP → Dolby Atmos Renderer puerto 8001

| Comando OSC | Función |
|-------------|---------|
| `/dar/monitoring/attenuation f 0.0-1.0` | Volumen (1.0 = 0dB, 0.0 = -inf) |
| `/dar/monitoring/dim i 0/1` | Dim on/off (-20dB interno del Renderer) |
| `/dar/monitoring/mute i 0/1` | **No implementado** en el Renderer — el mute se simula con attenuation=0.0 |

## Configuración por sala
En el .ino hay 4 valores a cambiar por sala:
```cpp
const char* WIFI_SSID     = "TU_RED_WIFI";
const char* WIFI_PASSWORD = "TU_PASSWORD";
const char* RENDERER_IP   = "192.168.1.XXX";  // IP del Mac con el Renderer
#define SPL_OFFSET  79.0f  // 0dB Renderer = X dB SPL (varía por sala)
```

## Comportamiento del CrowPanel
- **Encoder**: ±0.5 dB por detent
- **Click simple**: toggle mute
- **Doble click** (<300ms): reset a 0 dB, quita mute y dim
- **Long press** (≥600ms): toggle DIM
- **Pantalla**: SPL en grande, círculo amarillo/rojo/lila según estado
- **LEDs** (5x NeoPixel GPIO48): apagado=normal, rojo=mute, lila=DIM
- **Sleep**: pantalla apaga a los 5s, wake por toque táctil o encoder

## Instalar arduino-cli en un Mac nuevo (sin admin)
```bash
# Instalar arduino-cli
mkdir -p ~/bin
curl -L "https://github.com/arduino/arduino-cli/releases/download/v1.5.1/arduino-cli_1.5.1_macOS_ARM64.tar.gz" -o /tmp/arduino-cli.tar.gz
tar -xzf /tmp/arduino-cli.tar.gz -C ~/bin arduino-cli

# Configurar ESP32 core
~/bin/arduino-cli config init --overwrite
~/bin/arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
~/bin/arduino-cli core update-index
~/bin/arduino-cli core install esp32:esp32

# Instalar librerías manualmente (si downloads.arduino.cc no resuelve)
mkdir -p ~/Documents/Arduino/libraries
curl -L https://github.com/moononournation/Arduino_GFX/archive/refs/tags/v1.3.7.zip -o /tmp/gfx.zip
curl -L https://github.com/adafruit/Adafruit_NeoPixel/archive/refs/heads/master.zip -o /tmp/neopixel.zip
curl -L https://github.com/CNMAT/OSC/archive/refs/heads/master.zip -o /tmp/osc.zip
cd ~/Documents/Arduino/libraries
unzip -q /tmp/gfx.zip && mv Arduino_GFX-1.3.7 Arduino_GFX
unzip -q /tmp/neopixel.zip && mv Adafruit_NeoPixel-master Adafruit_NeoPixel
unzip -q /tmp/osc.zip && mv OSC-master OSC

# Instalar ctags (necesario para compilar .ino)
mkdir -p ~/Library/Arduino15/packages/builtin/tools/ctags/5.8-arduino11
curl -L --header "Authorization: Bearer QQ==" \
  "https://ghcr.io/v2/homebrew/core/ctags/blobs/sha256:614a735ab93afb5ed2a2f12a66819e0b35a1c644021670057d0cac0fbe9910ae" \
  -o /tmp/ctags.tar.gz
tar -xzf /tmp/ctags.tar.gz -C /tmp/
cp /tmp/ctags/5.8_2/bin/ctags ~/Library/Arduino15/packages/builtin/tools/ctags/5.8-arduino11/ctags
```

> **Nota**: Usar Arduino_GFX v1.3.7 (no la última) — la versión actual requiere ESP32 core 3.x.
> El hash de ctags es para macOS ARM64 Sequoia (15.x). Para otra versión buscar el hash en `https://formulae.brew.sh/api/formula/ctags.json`.

## Compilar y flashear
```bash
# Clonar repo
git clone https://github.com/vtort/CrowPanel_Dolby_Renderer.git
cd CrowPanel_Dolby_Renderer

# Editar credenciales (SSID, password, IP, SPL offset)
# nano CrowPanel_DADman.ino

# Compilar
~/bin/arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi" \
  --libraries ~/Documents/Arduino/libraries \
  CrowPanel_DADman.ino

# Ver puerto USB (conectar CrowPanel primero)
ls /dev/cu.*

# Flashear
~/bin/arduino-cli upload \
  --fqbn "esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi" \
  -p /dev/cu.usbmodemXXXXXX \
  CrowPanel_DADman.ino
```

> Si el CrowPanel no aparece en `/dev/cu.*`: usar cable con datos (no solo carga), conectar directamente al Mac (no hub), y si es la primera vez mantener BOOT pulsado al conectar.

## Salas configuradas
| Sala | SSID | IP Renderer | SPL offset |
|------|------|-------------|------------|
| 1 | TP-Link_E316 | 192.168.1.101 | 79 dB |
| 2 | TP-Link_DB24 | 192.168.1.103 | 82 dB |

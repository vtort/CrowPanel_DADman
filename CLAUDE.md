# CrowPanel DADman — Contexto del proyecto

## Qué es esto
Controlador de volumen master para DADman (software de audio para MTRX Studio) usando un CrowPanel ESP32-S3 1.28" (pantalla redonda). El jogwheel controla el volumen, la pantalla muestra el valor en dB SPL.

## Hardware
- **Board**: CrowPanel ESP32-S3 1.28" round display (GC9A01, 240x240)
- **FQBN arduino-cli**: `esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi`
- **Puerto**: `/dev/cu.usbmodem11101` (Mac) — puede variar
- **Flash**: BOOT+RST para entrar en bootloader, luego upload

## Pins críticos
- LCD_PWR_EN1 = GPIO1, LCD_PWR_EN2 = GPIO2 → deben estar en HIGH o la pantalla no arranca
- TFT_BL = GPIO46 → backlight via `analogWrite(46, 200)`
- ENC_A = 45, ENC_B = 42, ENC_SW = 41
- Touch CST816S I2C: SDA=6, SCL=7, RST=8, INT=0, addr=0x15

## Lógica de volumen
- DADman range: -100 a +12 dB
- SPL offset: +79 (0 dB DADman = 79 dB SPL, referencia near-field Dolby Atmos)
- Encoder: b==HIGH → aumenta volumen, 0.5 dB por detent (4 subpasos)
- MIDI CC7 canal 0 = volumen, Note 18 canal 0 = mute

## Estados del sprite (Pikachu Gen 2, 168x168px, RGB565)
- STATE_NORMAL (0): crystal_front → idle
- STATE_ACTIVE (1): silver_front → girando knob (vuelve a normal a los 400ms)
- STATE_MUTE (2): gold_back → mute

## Interacción botón
- **Click simple** → mute/unmute (en release)
- **Doble click** (< 300ms entre clicks) → reset al volumen de referencia (0 dB DADman = 79 dB SPL)
- **Long press** (≥ 600ms) → toggle DIM (-20 dB, guarda y restaura volumen previo)

## Display
- Fondo negro, círculo amarillo (radio 117-118), rojo en mute
- Valor SPL en grande arriba (textSize 4), label "dB SPL" debajo (gris)
- En mute: valor en rojo, label "MUTE" en rojo, círculo rojo
- Sprites: `draw16bitRGBBitmap` (little endian — importante, BeRGB da colores erróneos)

## Comandos flash
```bash
# Compilar y flashear (con board en bootloader: BOOT+RST)
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi" /ruta/al/proyecto
arduino-cli upload --fqbn "esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi" -p /dev/cu.usbmodem11101 /ruta/al/proyecto
```

## DADman configuración MIDI
- MIDI Mode: Mackie C4
- MIDI Input: ESP32 device (aparece como USB MIDI)
- CC7 canal 1 = master volume
- Note 18 canal 1 = mute toggle

## Funciones implementadas
- Encoder: 0.5 dB/detent, 4 subpasos
- Click simple → mute/unmute (en release)
- Doble click (<300ms) → reset a 0 dB DADman (79 dB SPL), sale de mute y DIM
- Long press (≥600ms) → toggle DIM (-20 dB, guarda y restaura volumen previo en preDimDB)
- LEDs NeoPixel (GPIO48, 5 LEDs): rojo=mute, lila=DIM, apagado=normal
- Sleep pantalla: apaga backlight tras 5s inactividad
- Wake: cualquier giro/click O toque táctil (CST816S polling I2C cada 100ms)
- Círculo amarillo en normal, lila en DIM, rojo en mute
- LEDs: apagados (normal), rojo (mute, RGB 255,0,0), lila (DIM, RGB 160,0,248). Mute tiene prioridad
- DIM: label "DIM" lila, círculo lila, valor lila, LEDs lila. Color RGB565: C_PURPLE = 0xA01F
- DIM guarda el volumen previo en preDimDB y lo restaura al salir

## MIDI - estado actual
- CrowPanel envía CC7 canal 1 al girar → controla DADman/Ableton ✅
- CrowPanel recibe CC7 canal 1 → actualiza valor en pantalla ✅ (testado con Ableton)
- Pendiente: confirmar si DADman manda feedback MIDI de vuelta (probar mañana en el curro)

## Pendiente
- Confirmar MIDI feedback de DADman (conectar y probar)
- Clip indicator: círculo parpadea rojo brevemente si volumen > umbral

## Repo
https://github.com/vtort/CrowPanel_DADman

// CrowPanel 1.28" Round - DADman Master Volume Controller
//
// Hardware: CrowPanel ESP32-S3, GC9A01 240x240 round display, rotary encoder
//
// Board settings (arduino-cli):
//   esp32:esp32:esp32s3:USBMode=usbotg,CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi
//
// Libraries: GFX Library for Arduino (Moon On Our Nation)

#include <Arduino_GFX_Library.h>
#include <Adafruit_NeoPixel.h>
#include "USB.h"
#include "USBMIDI.h"

#define LED_PIN   48
#define LED_COUNT  5
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ── Pins ──────────────────────────────────────────────────────────────────
#define TFT_SCLK     10
#define TFT_MOSI     11
#define TFT_DC        3
#define TFT_CS        9
#define TFT_RST      14
#define TFT_BL       46
#define LCD_PWR_EN1   1
#define LCD_PWR_EN2   2

#define ENC_A        45
#define ENC_B        42
#define ENC_SW       41

// ── MIDI ──────────────────────────────────────────────────────────────────
#define MIDI_CH       0    // Canal 1 (0-indexed)
#define CC_VOLUME     7
#define NOTE_MUTE    18

// ── Volumen ───────────────────────────────────────────────────────────────
#define DAD_MIN    -100.0f
#define DAD_MAX      12.0f
#define SPL_OFFSET  -79.0f
#define STEP_DB       0.5f

// ── Colores ───────────────────────────────────────────────────────────────
#define C_BG    0x0000
#define C_WHITE 0xFFFF
#define C_GRAY  0x8410
#define C_RED   0xF800
#define C_GREEN 0x07E0
#define C_YELLOW 0xFFE0

// ─────────────────────────────────────────────────────────────────────────

USBMIDI MidiUSB;

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, GFX_NOT_DEFINED, FSPI, true);
Arduino_GFX    *gfx  = new Arduino_GC9A01(bus, TFT_RST, 0, true);

float dadDB  = 0.0f;
bool  muted  = false;
int   lastA  = HIGH;
bool  lastBtn = HIGH;
unsigned long lastDebounce = 0;
bool needsRedraw = true;

// ─────────────────────────────────────────────────────────────────────────

void setup() {
  // LEDs apagados
  strip.begin();
  strip.clear();
  strip.show();

  // Power
  pinMode(LCD_PWR_EN1, OUTPUT);
  pinMode(LCD_PWR_EN2, OUTPUT);
  digitalWrite(LCD_PWR_EN1, HIGH);
  digitalWrite(LCD_PWR_EN2, HIGH);

  // Backlight
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 200);

  delay(20);

  // Display
  gfx->begin();
  gfx->fillScreen(C_BG);

  // Encoder
  pinMode(ENC_A,  INPUT_PULLUP);
  pinMode(ENC_B,  INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  lastA = digitalRead(ENC_A);

  // USB MIDI
  MidiUSB.begin();
  USB.begin();

  drawDisplay();
}

void loop() {
  handleEncoder();
  handleButton();
  if (needsRedraw) {
    drawDisplay();
    needsRedraw = false;
  }
}

// ── Encoder ───────────────────────────────────────────────────────────────

void handleEncoder() {
  int a = digitalRead(ENC_A);
  if (a == LOW && lastA == HIGH) {
    int b = digitalRead(ENC_B);
    if (b == LOW) {
      dadDB = min(DAD_MAX, dadDB + STEP_DB);
    } else {
      dadDB = max(DAD_MIN, dadDB - STEP_DB);
    }
    sendVolume();
    needsRedraw = true;
  }
  lastA = a;
}

// ── Botón ─────────────────────────────────────────────────────────────────

void handleButton() {
  bool btn = digitalRead(ENC_SW);
  if (btn != lastBtn && (millis() - lastDebounce) > 50) {
    lastDebounce = millis();
    if (btn == LOW) {
      muted = !muted;
      MidiUSB.noteOn(MIDI_CH, NOTE_MUTE, muted ? 127 : 0);
      needsRedraw = true;
    }
  }
  lastBtn = btn;
}

// ── MIDI ──────────────────────────────────────────────────────────────────

void sendVolume() {
  int ccVal = (int)((dadDB - DAD_MIN) / (DAD_MAX - DAD_MIN) * 127.0f);
  ccVal = constrain(ccVal, 0, 127);
  MidiUSB.controlChange(MIDI_CH, CC_VOLUME, ccVal);
}

// ── Display ───────────────────────────────────────────────────────────────

float getSPL() { return dadDB + SPL_OFFSET; }

uint16_t splColor() {
  if (muted) return C_RED;
  float spl = getSPL();
  if (spl > -20.0f) return C_RED;
  if (spl > -40.0f) return C_YELLOW;
  return C_GREEN;
}

void drawDisplay() {
  gfx->fillScreen(C_BG);

  float spl = getSPL();
  uint16_t col = splColor();

  // Valor SPL grande y centrado
  char buf[10];
  dtostrf(spl, 5, 1, buf);
  char *v = buf;
  while (*v == ' ') v++;

  gfx->setTextSize(4);
  gfx->setTextColor(col);
  int textW = strlen(v) * 24;
  gfx->setCursor((240 - textW) / 2, 80);
  gfx->print(v);

  // Unidad
  gfx->setTextSize(2);
  gfx->setTextColor(C_GRAY);
  gfx->setCursor(82, 128);
  gfx->print("dB SPL");

  // MUTE
  if (muted) {
    gfx->setTextSize(3);
    gfx->setTextColor(C_RED);
    gfx->setCursor(78, 158);
    gfx->print("MUTE");
  }

  // Borde circular
  gfx->drawCircle(120, 120, 118, col);
  gfx->drawCircle(120, 120, 117, col);
}

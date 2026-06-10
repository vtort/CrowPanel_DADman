// CrowPanel 1.28" Round - DADman Master Volume Controller
//
// Hardware: CrowPanel ESP32-S3, GC9A01 240x240 round display, rotary encoder
//
// Board settings (arduino-cli):
//   esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi
//
// Libraries: GFX Library for Arduino (Moon On Our Nation), Adafruit NeoPixel

#include <Arduino_GFX_Library.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "USB.h"
#include "USBMIDI.h"
#include "pikachu_sprites.h"

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

#define TOUCH_SDA     6
#define TOUCH_SCL     7
#define TOUCH_RST     8
#define TOUCH_INT     0
#define TOUCH_ADDR 0x15   // dirección I2C del CST816S

// ── MIDI ──────────────────────────────────────────────────────────────────
#define MIDI_CH       0
#define CC_VOLUME     7
#define NOTE_MUTE    18

// ── Volumen ───────────────────────────────────────────────────────────────
#define DAD_MIN    -100.0f
#define DAD_MAX      12.0f
#define SPL_OFFSET   79.0f

// ── Colores ───────────────────────────────────────────────────────────────
#define C_BG     0x0000
#define C_GRAY   0x8410
#define C_RED    0xF800
#define C_YELLOW 0xFFE0

// ── Sprite posiciones por estado
#define SPRITE_X_NORMAL  46
#define SPRITE_Y_NORMAL  58
#define SPRITE_X_MUTE    36
#define SPRITE_Y_MUTE    68

// ── Estados del sprite ────────────────────────────────────────────────────
#define STATE_NORMAL  0
#define STATE_ACTIVE  1   // girando el knob
#define STATE_MUTE    2

// ─────────────────────────────────────────────────────────────────────────

USBMIDI MidiUSB;

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, GFX_NOT_DEFINED, FSPI, true);
Arduino_GFX    *gfx  = new Arduino_GC9A01(bus, TFT_RST, 0, true);

float dadDB   = 0.0f;
bool  muted   = false;
bool  lastBtn = HIGH;
unsigned long lastDebounce  = 0;
unsigned long lastActiveMs  = 0;   // para volver a normal tras girar
unsigned long lastActivityMs = 0;  // para apagar pantalla por inactividad
bool screenOn = true;
unsigned long lastClickMs   = 0;   // para detectar doble click
bool          waitingDouble = false;
bool needsRedraw   = true;
bool firstDraw     = true;
uint8_t currentState = STATE_NORMAL;

// Encoder
int8_t  encAccum = 0;
uint8_t encState = 0;
const int8_t encTable[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};

// ─────────────────────────────────────────────────────────────────────────

void setup() {
  strip.begin();
  strip.clear();
  strip.show();

  pinMode(LCD_PWR_EN1, OUTPUT);
  pinMode(LCD_PWR_EN2, OUTPUT);
  digitalWrite(LCD_PWR_EN1, HIGH);
  digitalWrite(LCD_PWR_EN2, HIGH);

  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 200);

  delay(20);

  gfx->begin();
  gfx->fillScreen(C_BG);

  pinMode(ENC_A,    INPUT_PULLUP);
  pinMode(ENC_B,    INPUT_PULLUP);
  pinMode(ENC_SW,   INPUT_PULLUP);
  pinMode(TOUCH_INT, INPUT_PULLUP);

  // Inicializar CST816S
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);  delay(10);
  digitalWrite(TOUCH_RST, HIGH); delay(50);
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  encState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);

  MidiUSB.begin();
  USB.begin();

  drawDisplay();
}

void loop() {
  handleEncoder();
  handleButton();

  // Volver a normal tras dejar de girar
  if (currentState == STATE_ACTIVE && !muted && millis() - lastActiveMs > 150) {
    currentState = STATE_NORMAL;
    needsRedraw  = true;
  }


  // Apagar pantalla tras 5s de inactividad
  if (screenOn && millis() - lastActivityMs > 5000) {
    analogWrite(TFT_BL, 0);
    screenOn = false;
  }

  // Toque en pantalla → encender sin cambiar nada (polling I2C cada 100ms)
  static unsigned long lastTouchPoll = 0;
  if (!screenOn && millis() - lastTouchPoll > 100) {
    lastTouchPoll = millis();
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(0x02);  // registro finger_num
    if (Wire.endTransmission(false) == 0) {
      Wire.requestFrom(TOUCH_ADDR, 1);
      uint8_t fingers = Wire.available() ? Wire.read() : 0;
      if (fingers > 0) {
        lastActivityMs = millis();
        analogWrite(TFT_BL, 200);
        screenOn    = true;
        firstDraw   = true;
        needsRedraw = true;
      }
    }
  }

  if (needsRedraw) {
    // Si la pantalla estaba apagada, encenderla y redibujar todo
    if (!screenOn) {
      analogWrite(TFT_BL, 200);
      screenOn  = true;
      firstDraw = true;
    }
    drawDisplay();
    needsRedraw = false;
  }
}

// ── Encoder ───────────────────────────────────────────────────────────────

void handleEncoder() {
  uint8_t a = digitalRead(ENC_A);
  uint8_t b = digitalRead(ENC_B);
  uint8_t newState = (a << 1) | b;
  if (newState != (encState & 0x03)) {
    encState = ((encState << 2) | newState) & 0x0F;
    encAccum += encTable[encState];

    if (encAccum >= 4) {
      dadDB = constrain(round((dadDB + 0.5f) * 2.0f) / 2.0f, DAD_MIN, DAD_MAX);
      encAccum     = 0;
      lastActiveMs   = millis();
      lastActivityMs = millis();
      if (!muted) currentState = STATE_ACTIVE;
      sendVolume();
      needsRedraw = true;
    } else if (encAccum <= -4) {
      dadDB = constrain(round((dadDB - 0.5f) * 2.0f) / 2.0f, DAD_MIN, DAD_MAX);
      encAccum       = 0;
      lastActiveMs   = millis();
      lastActivityMs = millis();
      if (!muted) currentState = STATE_ACTIVE;
      sendVolume();
      needsRedraw = true;
    }
  }
}

// ── Botón ─────────────────────────────────────────────────────────────────

void handleButton() {
  bool btn = digitalRead(ENC_SW);
  if (btn != lastBtn && (millis() - lastDebounce) > 50) {
    lastDebounce = millis();
    if (btn == LOW) {
      unsigned long now = millis();
      if (waitingDouble && (now - lastClickMs) < 300) {
        // ── Doble click: deshacer mute anterior y hacer reset
        waitingDouble = false;
        muted  = false;
        dadDB  = 0.0f;
        MidiUSB.noteOn(MIDI_CH, NOTE_MUTE, 0);
        currentState = STATE_NORMAL;
        sendVolume();
        firstDraw   = true;
        needsRedraw = true;
      } else {
        // ── Primer click: mute inmediato
        waitingDouble  = true;
        lastClickMs    = now;
        lastActivityMs = now;
        muted = !muted;
        currentState = muted ? STATE_MUTE : STATE_NORMAL;
        MidiUSB.noteOn(MIDI_CH, NOTE_MUTE, muted ? 127 : 0);
        firstDraw   = true;
        needsRedraw = true;
      }
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

void drawSprite() {
  const uint16_t *sprite;
  int sx, sy;
  if (currentState == STATE_MUTE) {
    sprite = pikachu_mute;
    sx = SPRITE_X_MUTE; sy = SPRITE_Y_MUTE;
  } else if (currentState == STATE_ACTIVE) {
    sprite = pikachu_active;
    sx = SPRITE_X_NORMAL; sy = SPRITE_Y_NORMAL;
  } else {
    sprite = pikachu_normal;
    sx = SPRITE_X_NORMAL; sy = SPRITE_Y_NORMAL;
  }
  gfx->draw16bitRGBBitmap(sx, sy, (uint16_t*)sprite, PIKACHU_NORMAL_SIZE, PIKACHU_NORMAL_SIZE);
}

void drawDisplay() {
  if (firstDraw) {
    gfx->fillScreen(C_BG);
    firstDraw = false;
  }

  // 2. Sprite
  drawSprite();

  // 2. Valor SPL
  gfx->fillRect(30, 28, 180, 36, C_BG);
  char buf[10];
  dtostrf(getSPL(), 5, 1, buf);
  char *v = buf;
  while (*v == ' ') v++;
  gfx->setTextSize(4);
  gfx->setTextColor(muted ? C_RED : C_YELLOW);
  int textW = strlen(v) * 24;
  gfx->setCursor((240 - textW) / 2, 30);
  gfx->print(v);

  // 3. dB SPL / MUTE — siempre después del sprite, con bg negro (sin parpadeo)
  if (muted) {
    gfx->setTextSize(2);
    gfx->setTextColor(C_RED, C_BG);
    gfx->setCursor(92, 64);
    gfx->print("MUTE  ");  // espacios para borrar texto anterior
  } else {
    gfx->setTextSize(1);
    gfx->setTextColor(C_GRAY, C_BG);
    gfx->setCursor(95, 66);
    gfx->print("dB SPL");
  }

  // 5. Círculo SIEMPRE AL FINAL
  uint16_t circleColor = muted ? C_RED : C_YELLOW;
  gfx->drawCircle(120, 120, 118, circleColor);
  gfx->drawCircle(120, 120, 117, circleColor);
}

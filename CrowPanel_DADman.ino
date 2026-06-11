// CrowPanel 1.28" Round - Dolby Atmos Renderer Master Volume Controller
//
// Hardware: CrowPanel ESP32-S3, GC9A01 240x240 round display, rotary encoder
// Control:  OSC over WiFi → Dolby Atmos Renderer puerto 8001
//
// Board settings (arduino-cli):
//   esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi
//
// Libraries: GFX Library for Arduino (Moon On Our Nation), Adafruit NeoPixel, OSC (CNMAT)

#include <Arduino_GFX_Library.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include "pikachu_sprites.h"

// Prototipos
void setup();
void loop();
void updateLEDs();
void sendOSC_attenuation();
void sendOSC_mute(bool on);
void sendOSC_dim(bool on);
void handleEncoder();
void handleButton();
float getSPL();
void drawSprite();
void drawDisplay();

// ── WiFi / OSC ────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "TU_RED_WIFI";       // <- cambia esto
const char* WIFI_PASSWORD = "TU_PASSWORD";        // <- cambia esto
const char* RENDERER_IP   = "192.168.1.XXX";      // <- IP del Mac con el Renderer
const int   OSC_PORT      = 8001;

WiFiUDP Udp;

// ── LEDs ──────────────────────────────────────────────────────────────────
#define LED_PIN    48
#define LED_COUNT   5
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
#define TOUCH_ADDR 0x15

// ── Volumen ───────────────────────────────────────────────────────────────
#define DAD_MIN    -100.0f
#define DAD_MAX       0.0f   // el Renderer solo atenúa, max = 0 dB
#define SPL_OFFSET   79.0f

// ── Colores ───────────────────────────────────────────────────────────────
#define C_BG     0x0000
#define C_GRAY   0x8410
#define C_RED    0xF800
#define C_YELLOW 0xFFE0
#define C_PURPLE 0xA01F

// ── Sprite posiciones por estado ──────────────────────────────────────────
#define SPRITE_X_NORMAL  46
#define SPRITE_Y_NORMAL  58
#define SPRITE_X_MUTE    36
#define SPRITE_Y_MUTE    68

#define STATE_NORMAL  0
#define STATE_ACTIVE  1
#define STATE_MUTE    2

// ─────────────────────────────────────────────────────────────────────────

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, GFX_NOT_DEFINED, FSPI, true);
Arduino_GFX    *gfx  = new Arduino_GC9A01(bus, TFT_RST, 0, true);

float dadDB   = 0.0f;
bool  muted   = false;
bool  dimmed  = false;

bool          lastBtn        = HIGH;
unsigned long lastDebounce   = 0;
unsigned long lastActiveMs   = 0;
unsigned long lastActivityMs = 0;
bool          screenOn       = true;
unsigned long lastClickMs    = 0;
bool          waitingDouble  = false;
unsigned long pressStart     = 0;
bool          pressing       = false;
bool          needsRedraw    = true;
bool          firstDraw      = true;
uint8_t       currentState   = STATE_NORMAL;

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

  pinMode(ENC_A,     INPUT_PULLUP);
  pinMode(ENC_B,     INPUT_PULLUP);
  pinMode(ENC_SW,    INPUT_PULLUP);
  pinMode(TOUCH_INT, INPUT_PULLUP);

  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);  delay(10);
  digitalWrite(TOUCH_RST, HIGH); delay(50);
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  encState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);

  // ── WiFi ────────────────────────────────────────────────────────────────
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Muestra en pantalla que está conectando
  gfx->setTextSize(1);
  gfx->setTextColor(C_GRAY);
  gfx->setCursor(60, 115);
  gfx->print("Conectando WiFi...");
  while (WiFi.status() != WL_CONNECTED) delay(300);
  Udp.begin(8000);  // puerto local de escucha (no se usa, solo para inicializar)

  gfx->fillScreen(C_BG);
  drawDisplay();
}

void loop() {
  handleEncoder();
  handleButton();

  if (currentState == STATE_ACTIVE && !muted && millis() - lastActiveMs > 150) {
    currentState = STATE_NORMAL;
    needsRedraw  = true;
  }

  if (screenOn && millis() - lastActivityMs > 5000) {
    analogWrite(TFT_BL, 0);
    screenOn = false;
  }

  static unsigned long lastTouchPoll = 0;
  if (!screenOn && millis() - lastTouchPoll > 100) {
    lastTouchPoll = millis();
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(0x02);
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
    if (!screenOn) {
      analogWrite(TFT_BL, 200);
      screenOn  = true;
      firstDraw = true;
    }
    drawDisplay();
    needsRedraw = false;
  }
}

// ── LEDs ──────────────────────────────────────────────────────────────────

void updateLEDs() {
  if (muted) {
    for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, strip.Color(255, 0, 0));
  } else if (dimmed) {
    for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, strip.Color(160, 0, 248));
  } else {
    strip.clear();
  }
  strip.show();
}

// ── OSC ───────────────────────────────────────────────────────────────────

void sendOSC_attenuation() {
  // Convierte dB a amplitud lineal (0.0 - 1.0)
  // dadDB máximo es 0 dB → amplitude 1.0
  float amplitude = pow(10.0f, dadDB / 20.0f);
  amplitude = constrain(amplitude, 0.0f, 1.0f);

  OSCMessage msg("/dar/monitoring/attenuation");
  msg.add(amplitude);
  Udp.beginPacket(RENDERER_IP, OSC_PORT);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

void sendOSC_mute(bool on) {
  // El Renderer no implementa OSC mute — simulamos con atenuación 0.0 (-inf dB)
  float amplitude = on ? 0.0f : pow(10.0f, dadDB / 20.0f);
  amplitude = constrain(amplitude, 0.0f, 1.0f);
  OSCMessage msg("/dar/monitoring/attenuation");
  msg.add(amplitude);
  Udp.beginPacket(RENDERER_IP, OSC_PORT);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

void sendOSC_dim(bool on) {
  OSCMessage msg("/dar/monitoring/dim");
  msg.add((int32_t)(on ? 1 : 0));
  Udp.beginPacket(RENDERER_IP, OSC_PORT);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
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
      encAccum       = 0;
      lastActiveMs   = millis();
      lastActivityMs = millis();
      if (!muted) currentState = STATE_ACTIVE;
      sendOSC_attenuation();
      needsRedraw = true;
    } else if (encAccum <= -4) {
      dadDB = constrain(round((dadDB - 0.5f) * 2.0f) / 2.0f, DAD_MIN, DAD_MAX);
      encAccum       = 0;
      lastActiveMs   = millis();
      lastActivityMs = millis();
      if (!muted) currentState = STATE_ACTIVE;
      sendOSC_attenuation();
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
      pressStart = millis();
      pressing   = true;
    } else {
      unsigned long held = millis() - pressStart;
      pressing = false;
      if (held >= 600) {
        // Long press: toggle DIM (el Renderer aplica -20 dB internamente)
        lastActivityMs = millis();
        dimmed = !dimmed;
        sendOSC_dim(dimmed);
        updateLEDs();
        firstDraw   = true;
        needsRedraw = true;
      } else {
        unsigned long now = millis();
        if (waitingDouble && (now - lastClickMs) < 300) {
          // Doble click: reset a 0 dB, quitar mute y dim
          waitingDouble = false;
          muted         = false;
          dimmed        = false;
          dadDB         = 0.0f;
          currentState  = STATE_NORMAL;
          sendOSC_mute(false);
          sendOSC_dim(false);
          sendOSC_attenuation();
          updateLEDs();
          firstDraw   = true;
          needsRedraw = true;
        } else {
          // Click simple: toggle mute
          waitingDouble  = true;
          lastClickMs    = now;
          lastActivityMs = now;
          muted = !muted;
          currentState = muted ? STATE_MUTE : STATE_NORMAL;
          sendOSC_mute(muted);
          updateLEDs();
          firstDraw   = true;
          needsRedraw = true;
        }
      }
    }
  }
  lastBtn = btn;
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

  drawSprite();

  gfx->fillRect(30, 28, 180, 36, C_BG);
  char buf[10];
  dtostrf(getSPL(), 5, 1, buf);
  char *v = buf;
  while (*v == ' ') v++;
  gfx->setTextSize(4);
  gfx->setTextColor(muted ? C_RED : (dimmed ? C_PURPLE : C_YELLOW));
  int textW = strlen(v) * 24;
  gfx->setCursor((240 - textW) / 2, 30);
  gfx->print(v);

  if (muted) {
    gfx->setTextSize(2);
    gfx->setTextColor(C_RED, C_BG);
    gfx->setCursor(92, 64);
    gfx->print("MUTE  ");
  } else if (dimmed) {
    gfx->setTextSize(2);
    gfx->setTextColor(C_PURPLE, C_BG);
    gfx->setCursor(100, 64);
    gfx->print("DIM   ");
  } else {
    gfx->setTextSize(1);
    gfx->setTextColor(C_GRAY, C_BG);
    gfx->setCursor(95, 66);
    gfx->print("dB SPL");
  }

  uint16_t circleColor = muted ? C_RED : (dimmed ? C_PURPLE : C_YELLOW);
  gfx->drawCircle(120, 120, 118, circleColor);
  gfx->drawCircle(120, 120, 117, circleColor);
}

// Test pantalla CrowPanel 1.28" - con power enable correcto
#include <Arduino_GFX_Library.h>

#define TFT_SCLK     10
#define TFT_MOSI     11
#define TFT_DC        3
#define TFT_CS        9
#define TFT_RST      14
#define TFT_BL       46
#define LCD_PWR_EN1   1   // Power enable 1 - NECESARIO
#define LCD_PWR_EN2   2   // Power enable 2 - NECESARIO

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, GFX_NOT_DEFINED, FSPI, true);
Arduino_GFX    *gfx  = new Arduino_GC9A01(bus, TFT_RST, 0, true);

void setup() {
  // Alimentar la pantalla
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

  gfx->fillScreen(RGB565_RED);
  delay(1000);
  gfx->fillScreen(RGB565_GREEN);
  delay(1000);
  gfx->fillScreen(RGB565_BLUE);
  delay(1000);
  gfx->fillScreen(RGB565_BLACK);

  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(55, 105);
  gfx->print("HOLA!");
}

void loop() {}

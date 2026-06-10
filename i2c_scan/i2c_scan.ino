// Scan I2C y muestra resultado en pantalla
#include <Arduino_GFX_Library.h>
#include <Wire.h>

#define TFT_SCLK 10, TFT_MOSI 11, TFT_DC 3, TFT_CS 9, TFT_RST 14, TFT_BL 46
#define LCD_PWR_EN1 1
#define LCD_PWR_EN2 2

Arduino_DataBus *bus = new Arduino_ESP32SPI(3, 9, 10, 11, GFX_NOT_DEFINED, FSPI, true);
Arduino_GFX    *gfx = new Arduino_GC9A01(bus, 14, 0, true);

void setup() {
  pinMode(LCD_PWR_EN1, OUTPUT); digitalWrite(LCD_PWR_EN1, HIGH);
  pinMode(LCD_PWR_EN2, OUTPUT); digitalWrite(LCD_PWR_EN2, HIGH);
  pinMode(46, OUTPUT); analogWrite(46, 200);
  delay(20);
  gfx->begin();
  gfx->fillScreen(0x0000);
  gfx->setTextColor(0xFFFF);
  gfx->setTextSize(2);

  // Probar pines I2C más comunes
  int sdaPins[] = {4, 6, 21};
  int sclPins[] = {5, 7, 22};
  int found_addr = 0, found_sda = 0, found_scl = 0;

  for (int p = 0; p < 3; p++) {
    Wire.begin(sdaPins[p], sclPins[p]);
    for (int addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        found_addr = addr;
        found_sda  = sdaPins[p];
        found_scl  = sclPins[p];
      }
    }
    Wire.end();
    if (found_addr) break;
  }

  gfx->setCursor(20, 80);
  if (found_addr) {
    gfx->setTextColor(0x07E0); // verde
    gfx->print("FOUND!");
    gfx->setCursor(20, 110);
    gfx->setTextColor(0xFFFF);
    gfx->print("Addr: 0x");
    gfx->print(found_addr, HEX);
    gfx->setCursor(20, 140);
    gfx->print("SDA: ");
    gfx->print(found_sda);
    gfx->setCursor(20, 170);
    gfx->print("SCL: ");
    gfx->print(found_scl);
  } else {
    gfx->setTextColor(0xF800); // rojo
    gfx->print("NOT FOUND");
    gfx->setCursor(20, 110);
    gfx->setTextColor(0xFFFF);
    gfx->print("No I2C device");
  }
}

void loop() {}

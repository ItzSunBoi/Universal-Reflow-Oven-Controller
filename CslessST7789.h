#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <SPI.h>

// Minimal ST7789 transport for the 240x240, seven-pin module with no exposed
// chip-select pin. This deliberately reproduces the command sequence and SPI
// mode used by the known-working MicroPython driver supplied for the module.
class CslessST7789 : public Adafruit_GFX {
 public:
  CslessST7789(SPIClass &spi, int8_t dcPin, int8_t resetPin);

  bool begin(int8_t sckPin, int8_t mosiPin, uint32_t frequencyHz,
             uint8_t rotation = 0, bool invert = true);

  void setSPISpeed(uint32_t frequencyHz);
  static uint16_t color565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red & 0xF8U) << 8) |
                                 ((green & 0xFCU) << 3) |
                                 (blue >> 3));
  }
  void setRotation(uint8_t rotation) override;
  void invertDisplay(bool invert) override;

  // Push an RGB565 rectangle from an off-screen framebuffer. `stridePixels`
  // is the number of pixels between successive source rows.
  void pushImage(int16_t x, int16_t y, int16_t w, int16_t h,
                 const uint16_t *pixels, int16_t stridePixels);

  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void startWrite() override;
  void writePixel(int16_t x, int16_t y, uint16_t color) override;
  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                     uint16_t color) override;
  void writeFastVLine(int16_t x, int16_t y, int16_t h,
                      uint16_t color) override;
  void writeFastHLine(int16_t x, int16_t y, int16_t w,
                      uint16_t color) override;
  void endWrite() override;

 private:
  SPIClass &spi_;
  int8_t dcPin_;
  int8_t resetPin_;
  uint32_t frequencyHz_ = 1000000UL;
  uint16_t startX_ = 0;
  uint16_t startY_ = 0;
  bool transactionOpen_ = false;
  bool initialized_ = false;

  void hardwareReset();
  void command(uint8_t commandByte, const uint8_t *data = nullptr,
               size_t length = 0);
  void commandInTransaction(uint8_t commandByte, const uint8_t *data = nullptr,
                            size_t length = 0);
  void setAddressWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void writeRepeatedColor(uint16_t color, uint32_t pixelCount);
  bool clipRect(int16_t &x, int16_t &y, int16_t &w, int16_t &h) const;
};

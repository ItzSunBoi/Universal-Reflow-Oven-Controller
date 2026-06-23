#include "CslessST7789.h"

#include <algorithm>

namespace {
constexpr uint8_t CMD_SWRESET = 0x01;
constexpr uint8_t CMD_SLPOUT  = 0x11;
constexpr uint8_t CMD_NORON   = 0x13;
constexpr uint8_t CMD_INVOFF  = 0x20;
constexpr uint8_t CMD_INVON   = 0x21;
constexpr uint8_t CMD_DISPON  = 0x29;
constexpr uint8_t CMD_CASET   = 0x2A;
constexpr uint8_t CMD_RASET   = 0x2B;
constexpr uint8_t CMD_RAMWR   = 0x2C;
constexpr uint8_t CMD_MADCTL  = 0x36;
constexpr uint8_t CMD_COLMOD  = 0x3A;

constexpr uint16_t PANEL_WIDTH = 240;
constexpr uint16_t PANEL_HEIGHT = 240;
constexpr size_t PIXEL_CHUNK = 64;
}  // namespace

CslessST7789::CslessST7789(SPIClass &spi, int8_t dcPin, int8_t resetPin)
    : Adafruit_GFX(PANEL_WIDTH, PANEL_HEIGHT),
      spi_(spi),
      dcPin_(dcPin),
      resetPin_(resetPin) {}

bool CslessST7789::begin(int8_t sckPin, int8_t mosiPin,
                         uint32_t frequencyHz, uint8_t requestedRotation,
                         bool invert) {
  frequencyHz_ = frequencyHz > 0 ? frequencyHz : 1000000UL;

  pinMode(dcPin_, OUTPUT);
  digitalWrite(dcPin_, HIGH);

  if (resetPin_ >= 0) {
    pinMode(resetPin_, OUTPUT);
    digitalWrite(resetPin_, HIGH);
  }

  // This display is write-only and has no controllable chip-select line.
  spi_.begin(sckPin, -1, mosiPin, -1);
  initialized_ = true;

  hardwareReset();

  // Reproduce the known-working MicroPython initialization order exactly.
  command(CMD_SWRESET);
  delay(150);

  command(CMD_SLPOUT);
  delay(10);

  const uint8_t pixelFormat = 0x05;  // 16-bit RGB565 on this module
  command(CMD_COLMOD, &pixelFormat, 1);
  delay(10);

  setRotation(requestedRotation);

  command(invert ? CMD_INVON : CMD_INVOFF);
  delay(10);

  command(CMD_NORON);
  delay(10);

  command(CMD_DISPON);
  delay(10);
  return true;
}

void CslessST7789::setSPISpeed(uint32_t frequencyHz) {
  if (frequencyHz > 0) frequencyHz_ = frequencyHz;
}

void CslessST7789::hardwareReset() {
  if (resetPin_ < 0) return;

  // Exact active-low reset sequence used by the known-working driver.
  digitalWrite(resetPin_, HIGH);
  delay(10);
  digitalWrite(resetPin_, LOW);
  delay(10);
  digitalWrite(resetPin_, HIGH);
  delay(10);
}

void CslessST7789::setRotation(uint8_t requestedRotation) {
  rotation = requestedRotation & 3U;
  _width = WIDTH;
  _height = HEIGHT;

  uint8_t madctl = 0x00;
  switch (rotation) {
    case 0:
      madctl = 0x00;
      startX_ = 0;
      startY_ = 0;
      break;
    case 1:
      madctl = 0xA0;
      startX_ = 320 - PANEL_WIDTH;  // 80-column controller-RAM offset
      startY_ = 0;
      break;
    case 2:
      madctl = 0xC0;
      startX_ = 0;
      startY_ = 320 - PANEL_HEIGHT; // 80-row controller-RAM offset
      break;
    case 3:
    default:
      madctl = 0x60;
      startX_ = 0;
      startY_ = 0;
      break;
  }

  if (initialized_) command(CMD_MADCTL, &madctl, 1);
}

void CslessST7789::invertDisplay(bool invert) {
  if (initialized_) command(invert ? CMD_INVON : CMD_INVOFF);
}

void CslessST7789::command(uint8_t commandByte, const uint8_t *data,
                           size_t length) {
  startWrite();
  commandInTransaction(commandByte, data, length);
  endWrite();
}

void CslessST7789::commandInTransaction(uint8_t commandByte,
                                        const uint8_t *data,
                                        size_t length) {
  digitalWrite(dcPin_, LOW);
  spi_.transfer(commandByte);

  if (data != nullptr && length > 0) {
    digitalWrite(dcPin_, HIGH);
    spi_.transferBytes(data, nullptr, static_cast<uint32_t>(length));
  }
}

void CslessST7789::startWrite() {
  if (transactionOpen_) return;

  // The no-CS module's proven configuration is CPOL=1, CPHA=0: SPI mode 2.
  spi_.beginTransaction(SPISettings(frequencyHz_, MSBFIRST, SPI_MODE2));
  transactionOpen_ = true;
}

void CslessST7789::endWrite() {
  if (!transactionOpen_) return;
  spi_.endTransaction();
  transactionOpen_ = false;
}

bool CslessST7789::clipRect(int16_t &x, int16_t &y,
                            int16_t &w, int16_t &h) const {
  if (w <= 0 || h <= 0 || x >= _width || y >= _height ||
      x + w <= 0 || y + h <= 0) {
    return false;
  }

  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > _width) w = _width - x;
  if (y + h > _height) h = _height - y;
  return w > 0 && h > 0;
}

void CslessST7789::setAddressWindow(uint16_t x, uint16_t y,
                                    uint16_t w, uint16_t h) {
  const uint16_t x0 = startX_ + x;
  const uint16_t y0 = startY_ + y;
  const uint16_t x1 = x0 + w - 1U;
  const uint16_t y1 = y0 + h - 1U;

  const uint8_t columns[4] = {
      static_cast<uint8_t>(x0 >> 8), static_cast<uint8_t>(x0),
      static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1)};
  const uint8_t rows[4] = {
      static_cast<uint8_t>(y0 >> 8), static_cast<uint8_t>(y0),
      static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1)};

  commandInTransaction(CMD_CASET, columns, sizeof(columns));
  commandInTransaction(CMD_RASET, rows, sizeof(rows));
  commandInTransaction(CMD_RAMWR);
  digitalWrite(dcPin_, HIGH);
}

void CslessST7789::writeRepeatedColor(uint16_t color,
                                      uint32_t pixelCount) {
  uint8_t buffer[PIXEL_CHUNK * 2];
  const uint8_t high = static_cast<uint8_t>(color >> 8);
  const uint8_t low = static_cast<uint8_t>(color);

  for (size_t i = 0; i < PIXEL_CHUNK; ++i) {
    buffer[i * 2] = high;
    buffer[i * 2 + 1] = low;
  }

  while (pixelCount > 0) {
    const uint32_t thisChunk =
        std::min<uint32_t>(pixelCount, static_cast<uint32_t>(PIXEL_CHUNK));
    spi_.transferBytes(buffer, nullptr, thisChunk * 2U);
    pixelCount -= thisChunk;
  }
}

void CslessST7789::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= _width || y >= _height) return;
  startWrite();
  writePixel(x, y, color);
  endWrite();
}

void CslessST7789::writePixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= _width || y >= _height) return;
  setAddressWindow(static_cast<uint16_t>(x), static_cast<uint16_t>(y), 1, 1);
  const uint8_t pixel[2] = {static_cast<uint8_t>(color >> 8),
                            static_cast<uint8_t>(color)};
  spi_.transferBytes(pixel, nullptr, sizeof(pixel));
}

void CslessST7789::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                 uint16_t color) {
  if (!clipRect(x, y, w, h)) return;
  setAddressWindow(static_cast<uint16_t>(x), static_cast<uint16_t>(y),
                   static_cast<uint16_t>(w), static_cast<uint16_t>(h));
  writeRepeatedColor(color, static_cast<uint32_t>(w) * h);
}

void CslessST7789::writeFastVLine(int16_t x, int16_t y, int16_t h,
                                  uint16_t color) {
  writeFillRect(x, y, 1, h, color);
}

void CslessST7789::writeFastHLine(int16_t x, int16_t y, int16_t w,
                                  uint16_t color) {
  writeFillRect(x, y, w, 1, color);
}

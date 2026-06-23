#include "UiManager.h"

#include <cmath>
#include <cstring>

#include "Config.h"

namespace {
constexpr int16_t SCREEN_W = 240;
constexpr int16_t SCREEN_H = 240;
constexpr int16_t BUTTON_Y = 215;
constexpr uint8_t VISIBLE_PROFILE_ROWS = 3;
constexpr uint8_t VISIBLE_EDIT_ROWS = 5;
constexpr char NAME_CHARSET[] =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

uint8_t clampCursor(uint8_t cursor, uint8_t count) {
  if (count == 0) return 0;
  return cursor < (count - 1U) ? cursor : static_cast<uint8_t>(count - 1U);
}

void formatTime(uint32_t seconds, char *buffer, size_t capacity) {
  snprintf(buffer, capacity, "%02lu:%02lu",
           static_cast<unsigned long>(seconds / 60UL),
           static_cast<unsigned long>(seconds % 60UL));
}

float profilePeakTargetC(const ReflowProfile &profile) {
  float peakC = 0.0f;
  for (uint8_t i = 0; i < profile.stageCount; ++i) {
    if (profile.stages[i].targetC > peakC) {
      peakC = profile.stages[i].targetC;
    }
  }
  return peakC;
}

uint8_t nextIdleDimSeconds(uint8_t current) {
  switch (current) {
    case 30U: return 60U;
    case 60U: return 120U;
    case 120U: return TFT_IDLE_TIMEOUT_DISABLED;
    default: return 30U;
  }
}

uint8_t nextIdleOffMinutes(uint8_t current) {
  switch (current) {
    case 5U: return 10U;
    case 10U: return 30U;
    case 30U: return TFT_IDLE_TIMEOUT_DISABLED;
    default: return 5U;
  }
}

uint8_t nextIdleDimPercent(uint8_t current) {
  switch (current) {
    case 10U: return 20U;
    case 20U: return 30U;
    case 30U: return 40U;
    default: return 10U;
  }
}

void formatIdleDimDelay(uint8_t seconds, char *buffer, size_t capacity) {
  if (seconds == TFT_IDLE_TIMEOUT_DISABLED) {
    strlcpy(buffer, "OFF", capacity);
  } else {
    snprintf(buffer, capacity, "%us", seconds);
  }
}

void formatIdleOffDelay(uint8_t minutes, char *buffer, size_t capacity) {
  if (minutes == TFT_IDLE_TIMEOUT_DISABLED) {
    strlcpy(buffer, "OFF", capacity);
  } else {
    snprintf(buffer, capacity, "%um", minutes);
  }
}
}  // namespace

UiManager::UiManager(CslessST7789 &display, ProfileStore &profiles,
                     ReflowEngine &engine, TemperatureSensor &sensor,
                     BacklightController &backlight, HeaterController &heater,
                     PidAutotuner &autotuner, OtaManager &ota)
    : display_(display), frame_(240, 240), profiles_(profiles),
      engine_(engine), sensor_(sensor), backlight_(backlight), heater_(heater),
      autotuner_(autotuner), ota_(ota) {}

void UiManager::begin() {
  applyTheme();

  if (PIN_BUZZER >= 0) {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, buzzerOffLevel());
  }

  calibrationWorkingC_ = profiles_.settings().temperatureOffsetC;
  manualSetpointC_ = 120.0f;
  cursor_ = profiles_.selectedIndex();
  lastInteractionMs_ = millis();
  wakeEventGuardUntilMs_ = 0;
  backlightState_ = BacklightState::ACTIVE;
  lastOtaActive_ = ota_.active();
  dirty_ = true;
}

void UiManager::applyTheme() {
  const UiTheme theme = static_cast<UiTheme>(profiles_.settings().themeId);
  switch (theme) {
    case UiTheme::EMBER:
      cBg_ = CslessST7789::color565(16, 10, 10);
      cPanel_ = CslessST7789::color565(36, 22, 21);
      cPanel2_ = CslessST7789::color565(52, 30, 27);
      cLine_ = CslessST7789::color565(106, 70, 62);
      cText_ = CslessST7789::color565(250, 240, 232);
      cMuted_ = CslessST7789::color565(188, 158, 145);
      cCyan_ = CslessST7789::color565(255, 157, 92);
      cGreen_ = CslessST7789::color565(112, 224, 151);
      cYellow_ = CslessST7789::color565(255, 210, 92);
      cOrange_ = CslessST7789::color565(255, 127, 67);
      cRed_ = CslessST7789::color565(255, 91, 91);
      cPurple_ = CslessST7789::color565(219, 136, 255);
      cBlue_ = CslessST7789::color565(112, 174, 255);
      break;
    case UiTheme::FOREST:
      cBg_ = CslessST7789::color565(8, 15, 13);
      cPanel_ = CslessST7789::color565(17, 35, 29);
      cPanel2_ = CslessST7789::color565(26, 50, 41);
      cLine_ = CslessST7789::color565(62, 96, 78);
      cText_ = CslessST7789::color565(235, 248, 239);
      cMuted_ = CslessST7789::color565(143, 177, 158);
      cCyan_ = CslessST7789::color565(75, 220, 184);
      cGreen_ = CslessST7789::color565(99, 232, 134);
      cYellow_ = CslessST7789::color565(232, 211, 93);
      cOrange_ = CslessST7789::color565(243, 151, 83);
      cRed_ = CslessST7789::color565(242, 98, 105);
      cPurple_ = CslessST7789::color565(177, 143, 248);
      cBlue_ = CslessST7789::color565(91, 166, 242);
      break;
    case UiTheme::MONO:
      cBg_ = CslessST7789::color565(8, 8, 10);
      cPanel_ = CslessST7789::color565(25, 26, 29);
      cPanel2_ = CslessST7789::color565(39, 41, 45);
      cLine_ = CslessST7789::color565(93, 97, 105);
      cText_ = CslessST7789::color565(245, 246, 248);
      cMuted_ = CslessST7789::color565(165, 168, 175);
      cCyan_ = CslessST7789::color565(220, 223, 228);
      cGreen_ = CslessST7789::color565(201, 224, 205);
      cYellow_ = CslessST7789::color565(232, 220, 180);
      cOrange_ = CslessST7789::color565(225, 190, 160);
      cRed_ = CslessST7789::color565(235, 128, 128);
      cPurple_ = CslessST7789::color565(200, 183, 230);
      cBlue_ = CslessST7789::color565(168, 196, 230);
      break;
    case UiTheme::OCEAN:
    default:
      cBg_ = CslessST7789::color565(10, 12, 18);
      cPanel_ = CslessST7789::color565(22, 27, 38);
      cPanel2_ = CslessST7789::color565(31, 37, 52);
      cLine_ = CslessST7789::color565(65, 76, 96);
      cText_ = CslessST7789::color565(235, 241, 250);
      cMuted_ = CslessST7789::color565(155, 166, 184);
      cCyan_ = CslessST7789::color565(74, 211, 238);
      cGreen_ = CslessST7789::color565(78, 220, 151);
      cYellow_ = CslessST7789::color565(245, 198, 76);
      cOrange_ = CslessST7789::color565(255, 143, 79);
      cRed_ = CslessST7789::color565(245, 92, 96);
      cPurple_ = CslessST7789::color565(170, 130, 255);
      cBlue_ = CslessST7789::color565(92, 145, 255);
      break;
  }
  frameValid_ = false;
  renderedPageValid_ = false;
  dirty_ = true;
}

void UiManager::update(uint32_t nowMs) {
  if (PIN_BUZZER >= 0 && buzzerOffMs_ != 0 &&
      static_cast<int32_t>(nowMs - buzzerOffMs_) >= 0) {
    digitalWrite(PIN_BUZZER, buzzerOffLevel());
    buzzerOffMs_ = 0;
  }

  syncPageToRunState();

  const bool otaActiveNow = ota_.active();
  if (lastOtaActive_ && !otaActiveNow) {
    restoreConfiguredBacklight();
    dirty_ = true;
  }
  lastOtaActive_ = otaActiveNow;

  updateIdleBacklight(nowMs);

  // Do not spend SPI bandwidth redrawing an invisible idle screen. A wake
  // event marks the page dirty so it is rendered immediately after restoring
  // the configured brightness.
  if (backlightState_ == BacklightState::OFF && !shouldStayFullyLit()) {
    return;
  }

  const bool dynamicPage = page_ == Page::HOME || page_ == Page::RUNNING ||
                           page_ == Page::RUN_INFO || page_ == Page::MANUAL ||
                           page_ == Page::COMPLETE || page_ == Page::FAULT ||
                           page_ == Page::PID_AUTOTUNE ||
                           page_ == Page::PID_AUTOTUNE_INFO ||
                           page_ == Page::OTA_UPDATE ||
                           page_ == Page::OTA_INFO ||
                           page_ == Page::FAULT_DETAIL;
  const uint32_t refreshInterval =
      (page_ == Page::OTA_UPDATE || page_ == Page::OTA_INFO)
          ? OTA_UI_REFRESH_INTERVAL_MS
          : UI_REFRESH_INTERVAL_MS;
  if (!dirty_ && (!dynamicPage ||
                  (nowMs - lastDrawMs_) < refreshInterval)) {
    return;
  }

  drawCurrentPage(nowMs);
  dirty_ = false;
  lastDrawMs_ = nowMs;
}

void UiManager::handleButton(const ButtonEvent &event, uint32_t nowMs) {
  // A press always wakes the display. When the backlight was fully off, the
  // first event is consumed so an unseen press cannot accidentally start a
  // reflow cycle or change a setting. Dimmed-screen presses still perform the
  // requested action after restoring normal brightness.
  if (registerInteractionAndWake(nowMs)) {
    dirty_ = true;
    return;
  }
  if (wakeEventGuardUntilMs_ != 0 &&
      static_cast<int32_t>(nowMs - wakeEventGuardUntilMs_) < 0) {
    return;
  }
  wakeEventGuardUntilMs_ = 0;

  if (event.action != ButtonAction::REPEAT) {
    beep();
  }

  switch (page_) {
    case Page::HOME: handleHome(event, nowMs); break;
    case Page::PROFILE_LIST: handleProfileList(event); break;
    case Page::PROFILE_DETAIL: handleProfileDetail(event, nowMs); break;
    case Page::PROFILE_EDIT: handleProfileEdit(event); break;
    case Page::STAGE_LIST: handleStageList(event); break;
    case Page::STAGE_EDIT: handleStageEdit(event); break;
    case Page::VALUE_EDIT: handleValueEdit(event); break;
    case Page::NAME_EDIT: handleNameEdit(event); break;
    case Page::RUNNING: handleRunning(event, nowMs); break;
    case Page::RUN_INFO: handleRunInfo(event, nowMs); break;
    case Page::COMPLETE: handleComplete(event, nowMs); break;
    case Page::MENU: handleMenu(event); break;
    case Page::MANUAL: handleManual(event, nowMs); break;
    case Page::CALIBRATION: handleCalibration(event); break;
    case Page::LOGS: handleLogs(event); break;
    case Page::SETTINGS: handleSettings(event); break;
    case Page::PID_AUTOTUNE: handlePidAutotune(event, nowMs); break;
    case Page::PID_AUTOTUNE_INFO: handlePidAutotuneInfo(event); break;
    case Page::OTA_UPDATE: handleOtaUpdate(event, nowMs); break;
    case Page::OTA_INFO: handleOtaInfo(event); break;
    case Page::ABOUT: handleAbout(event); break;
    case Page::FAULT: handleFault(event); break;
    case Page::FAULT_DETAIL: handleFaultDetail(event); break;
    case Page::DELETE_CONFIRM: handleDeleteConfirm(event); break;
  }
  dirty_ = true;
}

void UiManager::syncPageToRunState() {
  const RunState current = engine_.state();
  if (current == lastRunState_) {
    return;
  }
  lastRunState_ = current;

  if (current == RunState::FAULT) {
    page_ = Page::FAULT;
  } else if (current == RunState::RUNNING || current == RunState::PAUSED) {
    if (page_ != Page::RUN_INFO) page_ = Page::RUNNING;
  } else if (current == RunState::MANUAL) {
    page_ = Page::MANUAL;
  } else if (current == RunState::COMPLETE) {
    page_ = Page::COMPLETE;
  }
  dirty_ = true;
}

bool UiManager::shouldStayFullyLit() const {
  if (ota_.active() || autotuner_.active() ||
      ((page_ == Page::PID_AUTOTUNE || page_ == Page::PID_AUTOTUNE_INFO) &&
       autotuner_.state() != PidAutotuner::State::IDLE)) {
    return true;
  }
  switch (engine_.state()) {
    case RunState::RUNNING:
    case RunState::PAUSED:
    case RunState::MANUAL:
    case RunState::COMPLETE:
    case RunState::FAULT:
      return true;
    case RunState::IDLE:
    default:
      return false;
  }
}

void UiManager::restoreConfiguredBacklight() {
  backlight_.setPercent(profiles_.settings().backlightPercent);
  backlightState_ = BacklightState::ACTIVE;
}

void UiManager::updateIdleBacklight(uint32_t nowMs) {
  if (shouldStayFullyLit()) {
    // Keep safety-critical and active-process screens visible continuously.
    lastInteractionMs_ = nowMs;
    if (backlightState_ != BacklightState::ACTIVE) {
      restoreConfiguredBacklight();
      dirty_ = true;
    }
    return;
  }

  const SystemSettings &settings = profiles_.settings();
  const uint32_t idleMs = nowMs - lastInteractionMs_;

  if (settings.idleOffMinutes != TFT_IDLE_TIMEOUT_DISABLED) {
    const uint32_t offAfterMs =
        static_cast<uint32_t>(settings.idleOffMinutes) * 60UL * 1000UL;
    if (idleMs >= offAfterMs) {
      if (backlightState_ != BacklightState::OFF) {
        backlight_.off();
        backlightState_ = BacklightState::OFF;
      }
      return;
    }
  }

  if (settings.idleDimSeconds != TFT_IDLE_TIMEOUT_DISABLED) {
    const uint32_t dimAfterMs =
        static_cast<uint32_t>(settings.idleDimSeconds) * 1000UL;
    if (idleMs >= dimAfterMs) {
      const uint8_t dimPercent =
          min(settings.backlightPercent, settings.idleDimPercent);
      if (backlightState_ != BacklightState::DIMMED ||
          backlight_.percent() != dimPercent) {
        backlight_.setPercent(dimPercent);
        backlightState_ = BacklightState::DIMMED;
      }
      return;
    }
  }

  if (backlightState_ != BacklightState::ACTIVE) {
    restoreConfiguredBacklight();
    dirty_ = true;
  }
}

bool UiManager::registerInteractionAndWake(uint32_t nowMs) {
  lastInteractionMs_ = nowMs;
  const bool wasOff = backlightState_ == BacklightState::OFF;
  if (backlightState_ != BacklightState::ACTIVE) {
    restoreConfiguredBacklight();
    dirty_ = true;
  }
  if (wasOff) {
    wakeEventGuardUntilMs_ = nowMs + TFT_WAKE_EVENT_GUARD_MS;
  }
  return wasOff;
}

void UiManager::drawCurrentPage(uint32_t nowMs) {
  const bool pageChanged =
      !renderedPageValid_ || renderedPage_ != page_ || !frameValid_;

  // Clearing happens only in RAM. The LCD keeps showing the previous complete
  // frame until changed tiles are ready, eliminating the visible black flash
  // caused by clearing the physical panel before redrawing its widgets.
  frame_.fillScreen(cBg_);
  frame_.setTextWrap(false);
  switch (page_) {
    case Page::HOME: drawHome(); break;
    case Page::PROFILE_LIST: drawProfileList(); break;
    case Page::PROFILE_DETAIL: drawProfileDetail(); break;
    case Page::PROFILE_EDIT: drawProfileEdit(); break;
    case Page::STAGE_LIST: drawStageList(); break;
    case Page::STAGE_EDIT: drawStageEdit(); break;
    case Page::VALUE_EDIT: drawValueEdit(); break;
    case Page::NAME_EDIT: drawNameEdit(); break;
    case Page::RUNNING: drawRunning(nowMs); break;
    case Page::RUN_INFO: drawRunInfo(nowMs); break;
    case Page::COMPLETE: drawComplete(); break;
    case Page::MENU: drawMenu(); break;
    case Page::MANUAL: drawManual(); break;
    case Page::CALIBRATION: drawCalibration(); break;
    case Page::LOGS: drawLogs(); break;
    case Page::SETTINGS: drawSettings(); break;
    case Page::PID_AUTOTUNE: drawPidAutotune(nowMs); break;
    case Page::PID_AUTOTUNE_INFO: drawPidAutotuneInfo(nowMs); break;
    case Page::OTA_UPDATE: drawOtaUpdate(nowMs); break;
    case Page::OTA_INFO: drawOtaInfo(nowMs); break;
    case Page::ABOUT: drawAbout(); break;
    case Page::FAULT: drawFault(); break;
    case Page::FAULT_DETAIL: drawFaultDetail(); break;
    case Page::DELETE_CONFIRM: drawDeleteConfirm(); break;
  }

  flushFrame(pageChanged);
  renderedPage_ = page_;
  renderedPageValid_ = true;
}

uint32_t UiManager::hashTile(const uint16_t *buffer, int16_t x, int16_t y,
                             int16_t w, int16_t h) const {
  // FNV-1a over RGB565 words. It is inexpensive and lets us avoid transmitting
  // unchanged regions without retaining a second 115 kB framebuffer.
  uint32_t hash = 2166136261UL;
  for (int16_t row = 0; row < h; ++row) {
    const uint16_t *pixel = buffer + static_cast<int32_t>(y + row) * SCREEN_W + x;
    for (int16_t column = 0; column < w; ++column) {
      const uint16_t value = pixel[column];
      hash ^= static_cast<uint8_t>(value >> 8);
      hash *= 16777619UL;
      hash ^= static_cast<uint8_t>(value);
      hash *= 16777619UL;
    }
  }
  return hash;
}

void UiManager::flushFrame(bool forceFullFrame) {
  uint16_t *buffer = frame_.getBuffer();
  if (buffer == nullptr) {
    Serial.println("ERROR: UI framebuffer allocation failed");
    return;
  }

  constexpr int16_t tileSize = UI_DIRTY_TILE_SIZE;
  constexpr int16_t tileColumns = SCREEN_W / tileSize;
  constexpr int16_t tileRows = SCREEN_H / tileSize;

  if (forceFullFrame) {
    // One uninterrupted transfer replaces the old page without ever exposing
    // a deliberately cleared LCD frame.
    display_.pushImage(0, 0, SCREEN_W, SCREEN_H, buffer, SCREEN_W);
  }

  uint16_t tileIndex = 0;
  for (int16_t tileY = 0; tileY < tileRows; ++tileY) {
    const int16_t y = tileY * tileSize;
    bool changed[tileColumns] = {};

    for (int16_t tileX = 0; tileX < tileColumns; ++tileX, ++tileIndex) {
      const int16_t x = tileX * tileSize;
      const uint32_t hash = hashTile(buffer, x, y, tileSize, tileSize);
      changed[tileX] = !forceFullFrame && frameValid_ &&
                       tileHashes_[tileIndex] != hash;
      tileHashes_[tileIndex] = hash;
    }

    // Merge adjacent dirty tiles into one horizontal transfer. This lowers
    // command overhead and makes each visible update more coherent.
    int16_t tileX = 0;
    while (!forceFullFrame && tileX < tileColumns) {
      while (tileX < tileColumns && !changed[tileX]) ++tileX;
      if (tileX >= tileColumns) break;

      const int16_t runStart = tileX;
      while (tileX < tileColumns && changed[tileX]) ++tileX;
      const int16_t runTiles = tileX - runStart;
      const int16_t x = runStart * tileSize;
      const int16_t width = runTiles * tileSize;

      display_.pushImage(x, y, width, tileSize,
                         buffer + static_cast<int32_t>(y) * SCREEN_W + x,
                         SCREEN_W);
    }
  }

  frameValid_ = true;
}

uint8_t UiManager::fitText(const char *text, char *output, size_t capacity,
                           int16_t maxWidth, uint8_t preferredSize) {
  if (output == nullptr || capacity == 0) return 1;
  const char *source = text != nullptr ? text : "";
  strlcpy(output, source, capacity);

  uint8_t size = preferredSize > 0 ? preferredSize : 1;
  while (size > 1U &&
         static_cast<int32_t>(strlen(output)) * 6 * size > maxWidth) {
    --size;
  }

  const size_t maxChars = maxWidth > 0
                              ? static_cast<size_t>(maxWidth / (6 * size))
                              : 0U;
  const size_t length = strlen(output);
  if (length <= maxChars) return size;

  if (maxChars == 0U) {
    output[0] = '\0';
  } else if (maxChars <= 3U) {
    output[maxChars] = '\0';
  } else {
    output[maxChars - 3U] = '.';
    output[maxChars - 2U] = '.';
    output[maxChars - 1U] = '.';
    output[maxChars] = '\0';
  }
  return size;
}

void UiManager::drawFittedText(const char *text, int16_t x, int16_t y,
                               int16_t width, int16_t height,
                               uint8_t preferredSize, uint16_t color,
                               TextAlign alignment) {
  char fitted[96];
  const uint8_t size = fitText(text, fitted, sizeof(fitted), width,
                               preferredSize);
  const int16_t textWidth =
      static_cast<int16_t>(strlen(fitted) * 6U * size);
  int16_t cursorX = x;
  if (alignment == TextAlign::CENTER) {
    cursorX = x + (width - textWidth) / 2;
  } else if (alignment == TextAlign::RIGHT) {
    cursorX = x + width - textWidth;
  }
  const int16_t textHeight = static_cast<int16_t>(8U * size);
  const int16_t cursorY = y + ((height - textHeight) > 0
                                   ? (height - textHeight) / 2
                                   : 0);

  frame_.setTextSize(size);
  frame_.setTextColor(color);
  frame_.setCursor(cursorX, cursorY);
  frame_.print(fitted);
}

void UiManager::drawWrappedText(const char *text, int16_t x, int16_t y,
                                int16_t width, uint8_t maxLines,
                                uint8_t size, uint16_t color,
                                TextAlign alignment, int16_t lineGap) {
  if (text == nullptr || maxLines == 0U || size == 0U || width <= 0) return;

  char remaining[128];
  strlcpy(remaining, text, sizeof(remaining));
  char *cursor = remaining;
  const size_t maxChars = static_cast<size_t>(width / (6 * size));
  if (maxChars == 0U) return;

  for (uint8_t lineIndex = 0; lineIndex < maxLines && *cursor != '\0';
       ++lineIndex) {
    while (*cursor == ' ') ++cursor;
    if (*cursor == '\0') break;

    char line[96];
    const size_t remainingLength = strlen(cursor);
    if (lineIndex + 1U == maxLines || remainingLength <= maxChars) {
      fitText(cursor, line, sizeof(line), width, size);
      cursor += remainingLength;
    } else {
      size_t split = maxChars;
      while (split > 0U && cursor[split] != ' ') --split;
      if (split == 0U) split = maxChars;
      const size_t copyLength = split < sizeof(line) - 1U
                                    ? split
                                    : sizeof(line) - 1U;
      memcpy(line, cursor, copyLength);
      line[copyLength] = '\0';
      size_t lineLength = strlen(line);
      while (lineLength > 0U && line[lineLength - 1U] == ' ') {
        line[--lineLength] = '\0';
      }
      cursor += split;
    }

    drawFittedText(line, x,
                   y + lineIndex * (static_cast<int16_t>(8U * size) + lineGap),
                   width, static_cast<int16_t>(8U * size), size, color,
                   alignment);
  }
}

void UiManager::drawHeader(const char *title, const char *status,
                           uint16_t accent) {
  frame_.fillRoundRect(6, 5, 228, 27, 8, cPanel_);

  int16_t titleWidth = 212;
  if (status != nullptr) {
    char fittedStatus[32];
    fitText(status, fittedStatus, sizeof(fittedStatus), 80, 1);
    int16_t pillWidth = static_cast<int16_t>(strlen(fittedStatus) * 6 + 12);
    if (pillWidth < 30) pillWidth = 30;
    if (pillWidth > 92) pillWidth = 92;
    const int16_t pillX = 228 - pillWidth;
    frame_.fillRoundRect(pillX, 10, pillWidth, 16, 6,
                         accent == 0 ? cCyan_ : accent);
    drawFittedText(fittedStatus, pillX + 5, 10, pillWidth - 10, 16, 1, cBg_,
                   TextAlign::CENTER);
    titleWidth = pillX - 20;
  }

  drawFittedText(title, 14, 7, titleWidth, 23, 2, cText_, TextAlign::LEFT);
}

void UiManager::drawButtons(const char *left, const char *middle,
                            const char *right) {
  const char *labels[3] = {left, middle, right};
  const int16_t xs[3] = {7, 84, 161};
  for (uint8_t i = 0; i < 3; ++i) {
    frame_.fillRoundRect(xs[i], BUTTON_Y, 72, 20, 6, cPanel2_);
    frame_.drawRoundRect(xs[i], BUTTON_Y, 72, 20, 6, cLine_);
    drawFittedText(labels[i], xs[i] + 4, BUTTON_Y, 64, 20, 1, cText_,
                   TextAlign::CENTER);
  }
}

void UiManager::drawPanel(int16_t x, int16_t y, int16_t w, int16_t h,
                          bool selected, uint16_t accent) {
  frame_.fillRoundRect(x, y, w, h, 9, selected ? cPanel2_ : cPanel_);
  frame_.drawRoundRect(x, y, w, h, 9,
                     selected ? (accent == 0 ? cCyan_ : accent) : cLine_);
  if (selected) {
    frame_.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 8,
                       accent == 0 ? cCyan_ : accent);
  }
}

void UiManager::drawCentered(const char *text, int16_t y, uint8_t size,
                             uint16_t color) {
  drawFittedText(text, 6, y, SCREEN_W - 12,
                 static_cast<int16_t>(8U * size), size, color,
                 TextAlign::CENTER);
}

void UiManager::drawTemperature(float temperatureC, int16_t centerX,
                                int16_t y, uint8_t size, uint16_t color) {
  char value[16];
  if (std::isfinite(temperatureC)) {
    snprintf(value, sizeof(value), "%.1f", temperatureC);
  } else {
    strlcpy(value, "---.-", sizeof(value));
  }

  const uint8_t unitSize = (size / 2U) > 1U
                               ? static_cast<uint8_t>(size / 2U)
                               : 1U;
  const int16_t valueWidth = static_cast<int16_t>(strlen(value) * 6 * size);
  const int16_t degreeGap = (size / 2U) > 2U ? static_cast<int16_t>(size / 2U) : 2;
  const int16_t degreeDiameter = (unitSize + 2U) > 3U ? static_cast<int16_t>(unitSize + 2U) : 3;
  const int16_t cWidth = static_cast<int16_t>(6 * unitSize);
  const int16_t totalWidth = valueWidth + degreeGap + degreeDiameter + 2 + cWidth;
  const int16_t x = centerX - totalWidth / 2;

  frame_.setTextSize(size);
  frame_.setTextColor(color);
  frame_.setCursor(x, y);
  frame_.print(value);

  const int16_t degreeX = x + valueWidth + degreeGap;
  const int16_t radius = (degreeDiameter / 2) > 1 ? degreeDiameter / 2 : 1;
  frame_.drawCircle(degreeX + radius, y + radius + 1, radius, color);
  frame_.setTextSize(unitSize);
  frame_.setCursor(degreeX + degreeDiameter + 2, y + 2);
  frame_.print("C");
}

void UiManager::drawProgress(int16_t x, int16_t y, int16_t w, int16_t h,
                             float fraction, uint16_t color) {
  fraction = constrain(fraction, 0.0f, 1.0f);
  frame_.fillRoundRect(x, y, w, h, h / 2, CslessST7789::color565(18, 22, 31));
  frame_.drawRoundRect(x, y, w, h, h / 2, cLine_);
  const int16_t fillWidth = static_cast<int16_t>((w - 2) * fraction);
  if (fillWidth > 2) {
    frame_.fillRoundRect(x + 1, y + 1, fillWidth, h - 2,
                       ((h - 2) / 2) > 1 ? static_cast<int16_t>((h - 2) / 2) : 1, color);
  }
}

void UiManager::drawListRow(int16_t y, const char *primary,
                            const char *secondary, bool selected,
                            uint16_t dotColor) {
  drawPanel(12, y, 216, 44, selected, selected ? cCyan_ : 0);
  const int16_t textWidth = dotColor != 0 ? 169 : 192;
  drawFittedText(primary, 24, y + 4, textWidth, 23, 2, cText_,
                 TextAlign::LEFT);
  if (secondary != nullptr) {
    drawFittedText(secondary, 24, y + 26, textWidth, 15, 1, cMuted_,
                   TextAlign::LEFT);
  }
  if (dotColor != 0) {
    frame_.fillCircle(205, y + 21, 7, dotColor);
  }
}

void UiManager::drawScrollIndicator(uint8_t totalItems, uint8_t firstVisible,
                                    uint8_t visibleItems) {
  if (totalItems <= visibleItems || totalItems == 0) return;
  frame_.fillRoundRect(232, 43, 4, 160, 2, cPanel2_);
  const int16_t thumbHeight = (160 * visibleItems / totalItems) > 16 ? static_cast<int16_t>(160 * visibleItems / totalItems) : 16;
  const int16_t travel = 160 - thumbHeight;
  const int16_t maxFirst = totalItems - visibleItems;
  const int16_t thumbY = 43 + (maxFirst == 0 ? 0 : travel * firstVisible / maxFirst);
  frame_.fillRoundRect(232, thumbY, 4, thumbHeight, 2, cCyan_);
}

void UiManager::drawProfileGraph(const ReflowProfile &profile, int16_t x,
                                 int16_t y, int16_t w, int16_t h) {
  frame_.fillRoundRect(x, y, w, h, 8, CslessST7789::color565(13, 16, 23));
  frame_.drawRoundRect(x, y, w, h, 8, cLine_);
  for (uint8_t i = 1; i < 4; ++i) {
    const int16_t gy = y + i * h / 4;
    frame_.drawFastHLine(x + 4, gy, w - 8, CslessST7789::color565(35, 42, 56));
  }
  for (uint8_t i = 1; i < 5; ++i) {
    const int16_t gx = x + i * w / 5;
    frame_.drawFastVLine(gx, y + 4, h - 8, CslessST7789::color565(35, 42, 56));
  }

  uint32_t totalS = 0;
  for (uint8_t i = 0; i < profile.stageCount; ++i) {
    totalS += profile.stages[i].durationS;
  }
  if (totalS == 0) return;

  float previousTemp = 25.0f;
  uint32_t elapsedS = 0;
  int16_t previousX = x + 8;
  int16_t previousY = y + h - 8 -
                      static_cast<int16_t>(previousTemp / 270.0f * (h - 16));

  for (uint8_t i = 0; i < profile.stageCount; ++i) {
    const ReflowStage &stage = profile.stages[i];
    elapsedS += stage.durationS;
    const int16_t nextX = x + 8 +
                          static_cast<int16_t>(
                              static_cast<float>(elapsedS) / totalS * (w - 16));
    const float nextTemp = stage.targetC;
    const int16_t nextY = y + h - 8 -
                          static_cast<int16_t>(nextTemp / 270.0f * (h - 16));
    if (stage.mode == StageMode::HOLD) {
      const int16_t holdStartY = y + h - 8 -
          static_cast<int16_t>(stage.targetC / 270.0f * (h - 16));
      frame_.drawLine(previousX, holdStartY, nextX, holdStartY, cMuted_);
    } else {
      frame_.drawLine(previousX, previousY, nextX, nextY, cMuted_);
    }
    previousX = nextX;
    previousY = nextY;
    previousTemp = nextTemp;
  }
}

void UiManager::drawRunGraph(int16_t x, int16_t y, int16_t w, int16_t h) {
  frame_.fillRoundRect(x, y, w, h, 8, CslessST7789::color565(13, 16, 23));
  frame_.drawRoundRect(x, y, w, h, 8, cLine_);
  for (uint8_t i = 1; i < 4; ++i) {
    frame_.drawFastHLine(x + 4, y + i * h / 4, w - 8,
                       CslessST7789::color565(35, 42, 56));
  }
  for (uint8_t i = 1; i < 5; ++i) {
    frame_.drawFastVLine(x + i * w / 5, y + 4, h - 8,
                       CslessST7789::color565(35, 42, 56));
  }

  const uint16_t count = engine_.historyCount();
  if (count == 0) return;
  const float maxTemp = max(270.0f, engine_.activeProfile().maxTemperatureC + 10.0f);

  int16_t lastActualX = 0;
  int16_t lastActualY = 0;
  int16_t lastTargetX = 0;
  int16_t lastTargetY = 0;
  for (uint16_t i = 0; i < count; ++i) {
    const int16_t px = x + 8 +
        static_cast<int16_t>((count <= 1 ? 0.0f :
            static_cast<float>(i) / (count - 1U)) * (w - 16));
    const float actual = engine_.historyTemperature(i);
    const float target = engine_.historyTarget(i);
    const int16_t actualY = y + h - 8 - static_cast<int16_t>(
        constrain(actual / maxTemp, 0.0f, 1.0f) * (h - 16));
    const int16_t targetY = y + h - 8 - static_cast<int16_t>(
        constrain(target / maxTemp, 0.0f, 1.0f) * (h - 16));

    if (i > 0) {
      frame_.drawLine(lastTargetX, lastTargetY, px, targetY, cMuted_);
      frame_.drawLine(lastActualX, lastActualY, px, actualY, cCyan_);
      frame_.drawLine(lastActualX, lastActualY + 1, px, actualY + 1, cCyan_);
    }
    lastActualX = px;
    lastActualY = actualY;
    lastTargetX = px;
    lastTargetY = targetY;
  }
}

void UiManager::drawHome() {
  const bool ready = sensor_.valid() && engine_.state() == RunState::IDLE &&
                     !autotuner_.active() && !ota_.active();
  drawHeader("REFLOW OVEN", ready ? "READY" : "LOCKED",
             ready ? cGreen_ : cRed_);

  drawPanel(12, 42, 216, 82);
  const float temp = sensor_.valid() ? sensor_.temperatureC() : NAN;
  drawTemperature(temp, 120, 56, 4, sensor_.valid() ? cCyan_ : cRed_);
  drawCentered("Chamber temperature", 103, 1, cMuted_);

  drawPanel(12, 133, 216, 72);
  frame_.setTextSize(1);
  frame_.setTextColor(cMuted_);
  frame_.setCursor(24, 145);
  frame_.print("Selected profile");

  const ReflowProfile &profile = profiles_.selectedProfile();
  drawFittedText(profile.name, 24, 158, 192, 27, 2, cText_,
                 TextAlign::LEFT);

  char detail[48];
  snprintf(detail, sizeof(detail), "Peak %.1fC   TAL %us",
           profilePeakTargetC(profile),
           profile.targetTimeAboveLiquidusS);
  drawFittedText(detail, 24, 187, 192, 16, 1, cYellow_,
                 TextAlign::LEFT);

  drawButtons("PROFILE", ready ? "START" : "LOCKED", "MENU");
}

void UiManager::drawProfileList() {
  drawHeader("PROFILES");
  const bool canAddProfile = profiles_.profileCount() < MAX_PROFILES;
  const uint8_t total = profiles_.profileCount() +
                        (canAddProfile ? 1U : 0U);
  cursor_ = clampCursor(cursor_, total);
  uint8_t first = 0;
  if (cursor_ >= VISIBLE_PROFILE_ROWS) {
    first = cursor_ - VISIBLE_PROFILE_ROWS + 1U;
  }

  for (uint8_t row = 0; row < VISIBLE_PROFILE_ROWS; ++row) {
    const uint8_t index = first + row;
    if (index >= total) break;
    const int16_t y = 44 + row * 52;
    if (canAddProfile && index == profiles_.profileCount()) {
      drawListRow(y, "+ Add profile", "Duplicate then edit",
                  index == cursor_, cPurple_);
    } else {
      const ReflowProfile &profile = profiles_.profile(index);
      char secondary[40];
      snprintf(secondary, sizeof(secondary), "Liq %.1fC  max %.1fC",
               profile.liquidusC, profile.maxTemperatureC);
      const uint16_t dot = index == profiles_.selectedIndex() ? cGreen_ : cCyan_;
      drawListRow(y, profile.name, secondary, index == cursor_, dot);
    }
  }
  drawScrollIndicator(total, first, VISIBLE_PROFILE_ROWS);
  drawButtons("BACK", "SELECT", "DOWN");
}

void UiManager::drawProfileDetail() {
  const ReflowProfile &profile = profiles_.selectedProfile();
  drawHeader(profile.name);
  drawProfileGraph(profile, 12, 42, 216, 90);

  char line[48];
  frame_.setTextSize(1);
  frame_.setTextColor(cMuted_);
  frame_.setCursor(18, 142);
  frame_.print("Liquidus");
  snprintf(line, sizeof(line), "%.1fC", profile.liquidusC);
  frame_.setTextColor(cText_);
  frame_.setCursor(82, 142);
  frame_.print(line);

  frame_.setTextColor(cMuted_);
  frame_.setCursor(18, 159);
  frame_.print("Max limit");
  snprintf(line, sizeof(line), "%.1fC", profile.maxTemperatureC);
  frame_.setTextColor(cText_);
  frame_.setCursor(82, 159);
  frame_.print(line);

  frame_.setTextColor(cMuted_);
  frame_.setCursor(18, 176);
  frame_.print("Ramp limit");
  snprintf(line, sizeof(line), "%.1fC/s", profile.maxRampCPerSecond);
  frame_.setTextColor(cText_);
  frame_.setCursor(82, 176);
  frame_.print(line);

  frame_.setTextColor(cMuted_);
  frame_.setCursor(18, 193);
  frame_.print("Stages");
  snprintf(line, sizeof(line), "%u", profile.stageCount);
  frame_.setTextColor(cYellow_);
  frame_.setCursor(82, 193);
  frame_.print(line);

  const bool canStart = sensor_.valid() && engine_.state() == RunState::IDLE &&
                        !autotuner_.active() && !ota_.active();
  drawButtons("BACK", "EDIT", canStart ? "START" : "LOCKED");
}

void UiManager::drawProfileEdit() {
  drawHeader("EDIT PROFILE", "CUSTOM", cPurple_);
  static const char *labels[] = {
      "Name", "Liquidus", "Max temperature", "Ramp limit",
      "TAL target", "Stages", "Delete profile", "Save & back"};
  constexpr uint8_t itemCount = sizeof(labels) / sizeof(labels[0]);
  cursor_ = clampCursor(cursor_, itemCount);
  uint8_t first = 0;
  if (cursor_ >= VISIBLE_EDIT_ROWS) first = cursor_ - VISIBLE_EDIT_ROWS + 1U;

  for (uint8_t row = 0; row < VISIBLE_EDIT_ROWS; ++row) {
    const uint8_t index = first + row;
    if (index >= itemCount) break;
    const int16_t y = 44 + row * 32;
    drawPanel(12, y, 216, 27, index == cursor_);
    char value[32] = "";
    switch (index) {
      case 0: strlcpy(value, editProfile_.name, sizeof(value)); break;
      case 1: snprintf(value, sizeof(value), "%.1fC", editProfile_.liquidusC); break;
      case 2: snprintf(value, sizeof(value), "%.1fC", editProfile_.maxTemperatureC); break;
      case 3: snprintf(value, sizeof(value), "%.1fC/s", editProfile_.maxRampCPerSecond); break;
      case 4: snprintf(value, sizeof(value), "%us", editProfile_.targetTimeAboveLiquidusS); break;
      case 5: snprintf(value, sizeof(value), "%u", editProfile_.stageCount); break;
      case 6: strlcpy(value, profiles_.profileCount() > 1 ? "Available" : "Locked", sizeof(value)); break;
      case 7: strlcpy(value, "Save", sizeof(value)); break;
    }
    const int16_t valueX = index == 0 ? 96 : 142;
    drawFittedText(labels[index], 23, y, valueX - 29, 27, 1,
                   index == cursor_ ? cText_ : cMuted_, TextAlign::LEFT);
    drawFittedText(value, valueX, y, 216 - valueX, 27, 1,
                   index == 6 ? cRed_ : (index == 7 ? cGreen_ : cText_),
                   TextAlign::RIGHT);
  }
  drawScrollIndicator(itemCount, first, VISIBLE_EDIT_ROWS);
  drawButtons("BACK", "OPEN", "DOWN");
}

void UiManager::drawStageList() {
  drawHeader("PROFILE STAGES");
  const uint8_t total = editProfile_.stageCount +
                        (editProfile_.stageCount < MAX_PROFILE_STAGES ? 1U : 0U);
  cursor_ = clampCursor(cursor_, total);
  uint8_t first = 0;
  if (cursor_ >= VISIBLE_PROFILE_ROWS) first = cursor_ - VISIBLE_PROFILE_ROWS + 1U;

  for (uint8_t row = 0; row < VISIBLE_PROFILE_ROWS; ++row) {
    const uint8_t index = first + row;
    if (index >= total) break;
    const int16_t y = 44 + row * 52;
    if (index == editProfile_.stageCount) {
      drawListRow(y, "+ Add stage", "Ramp stage, 60 seconds",
                  index == cursor_, cPurple_);
    } else {
      const ReflowStage &stage = editProfile_.stages[index];
      char secondary[48];
      snprintf(secondary, sizeof(secondary), "%s  %.1fC  %us",
               stageModeName(stage.mode), stage.targetC, stage.durationS);
      uint16_t dot = stage.mode == StageMode::COOL ? cBlue_ :
                     stage.mode == StageMode::HOLD ? cYellow_ : cOrange_;
      drawListRow(y, stage.name, secondary, index == cursor_, dot);
    }
  }
  drawScrollIndicator(total, first, VISIBLE_PROFILE_ROWS);
  drawButtons("BACK", "EDIT", "DOWN");
}

void UiManager::drawStageEdit() {
  drawHeader("EDIT STAGE");
  static const char *labels[] = {
      "Name", "Mode", "Target", "Duration", "Move up",
      "Move down", "Delete", "Back"};
  constexpr uint8_t itemCount = sizeof(labels) / sizeof(labels[0]);
  cursor_ = clampCursor(cursor_, itemCount);
  uint8_t first = 0;
  if (cursor_ >= VISIBLE_EDIT_ROWS) first = cursor_ - VISIBLE_EDIT_ROWS + 1U;
  ReflowStage &stage = editProfile_.stages[editStageIndex_];

  for (uint8_t row = 0; row < VISIBLE_EDIT_ROWS; ++row) {
    const uint8_t index = first + row;
    if (index >= itemCount) break;
    const int16_t y = 44 + row * 32;
    drawPanel(12, y, 216, 27, index == cursor_);
    char value[32] = "";
    switch (index) {
      case 0: strlcpy(value, stage.name, sizeof(value)); break;
      case 1: strlcpy(value, stageModeName(stage.mode), sizeof(value)); break;
      case 2: snprintf(value, sizeof(value), "%.1fC", stage.targetC); break;
      case 3: snprintf(value, sizeof(value), "%us", stage.durationS); break;
      case 4: strlcpy(value, editStageIndex_ > 0 ? "Available" : "Top", sizeof(value)); break;
      case 5: strlcpy(value, editStageIndex_ + 1U < editProfile_.stageCount ? "Available" : "Bottom", sizeof(value)); break;
      case 6: strlcpy(value, editProfile_.stageCount > 1 ? "Delete" : "Locked", sizeof(value)); break;
      case 7: strlcpy(value, "Done", sizeof(value)); break;
    }
    const int16_t valueX = index == 0 ? 96 : 142;
    drawFittedText(labels[index], 23, y, valueX - 29, 27, 1,
                   index == cursor_ ? cText_ : cMuted_, TextAlign::LEFT);
    drawFittedText(value, valueX, y, 216 - valueX, 27, 1,
                   index == 6 ? cRed_ : (index == 7 ? cGreen_ : cText_),
                   TextAlign::RIGHT);
  }
  drawScrollIndicator(itemCount, first, VISIBLE_EDIT_ROWS);
  drawButtons("BACK", "OPEN", "DOWN");
}

void UiManager::drawValueEdit() {
  const char *title = "EDIT VALUE";
  const char *label = "Value";
  char value[32];
  switch (valueKind_) {
    case ValueKind::LIQUIDUS:
      title = "LIQUIDUS"; label = "Solder liquidus";
      snprintf(value, sizeof(value), "%.1f C", editProfile_.liquidusC); break;
    case ValueKind::MAX_TEMPERATURE:
      title = "MAX TEMPERATURE"; label = "Safety ceiling";
      snprintf(value, sizeof(value), "%.1f C", editProfile_.maxTemperatureC); break;
    case ValueKind::MAX_RAMP:
      title = "RAMP LIMIT"; label = "Maximum target ramp";
      snprintf(value, sizeof(value), "%.1f C/s", editProfile_.maxRampCPerSecond); break;
    case ValueKind::TAL_TARGET:
      title = "TAL TARGET"; label = "Above liquidus";
      snprintf(value, sizeof(value), "%u seconds", editProfile_.targetTimeAboveLiquidusS); break;
    case ValueKind::STAGE_TARGET:
      title = "STAGE TARGET"; label = editProfile_.stages[editStageIndex_].name;
      snprintf(value, sizeof(value), "%.1f C", editProfile_.stages[editStageIndex_].targetC); break;
    case ValueKind::STAGE_DURATION:
      title = "STAGE DURATION"; label = editProfile_.stages[editStageIndex_].name;
      snprintf(value, sizeof(value), "%u seconds", editProfile_.stages[editStageIndex_].durationS); break;
  }

  drawHeader(title);
  drawPanel(12, 49, 216, 116, true, cCyan_);
  drawCentered(value, 77, 3, cCyan_);
  drawCentered(label, 128, 1, cMuted_);
  drawCentered("Values are constrained for safety", 184, 1, cYellow_);
  drawButtons("-", "SAVE", "+");
}

void UiManager::drawNameEdit() {
  drawHeader(nameKind_ == NameKind::PROFILE ? "PROFILE NAME" : "STAGE NAME");
  drawPanel(12, 48, 216, 84, true, cPurple_);
  char *buffer = activeNameBuffer();
  char fittedName[32];
  const uint8_t nameSize = fitText(buffer, fittedName, sizeof(fittedName),
                                   196, 2);
  drawFittedText(fittedName, 22, 64, 196, 36, nameSize, cText_,
                 TextAlign::CENTER);

  const int16_t textWidth =
      static_cast<int16_t>(strlen(fittedName) * 6U * nameSize);
  const int16_t charX = 22 + (196 - textWidth) / 2 +
                        nameCursor_ * static_cast<int16_t>(6U * nameSize);
  frame_.drawFastHLine(charX, 101,
                       static_cast<int16_t>(6U * nameSize - 2U), cPurple_);
  frame_.setTextSize(1);
  frame_.setTextColor(cMuted_);
  frame_.setCursor(18, 148);
  frame_.print("Left/right change character");
  frame_.setCursor(18, 165);
  frame_.print("OK advances cursor");
  frame_.setTextColor(cYellow_);
  frame_.setCursor(18, 188);
  frame_.print("Hold OK to save name");
  const bool atLastNamePosition =
      nameCursor_ + 1U >= activeNameCapacity() - 1U;
  drawButtons("CHAR-", atLastNamePosition ? "SAVE" : "NEXT", "CHAR+");
}

void UiManager::drawRunning(uint32_t nowMs) {
  const bool paused = engine_.state() == RunState::PAUSED;
  drawHeader("RUNNING", paused ? "PAUSED" : engine_.stageName(),
             paused ? cPurple_ : cYellow_);

  drawPanel(12, 40, 100, 62);
  frame_.setTextSize(1);
  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 48);
  frame_.print("ACTUAL");
  drawTemperature(sensor_.valid() ? sensor_.temperatureC() : NAN,
                  62, 68, 2, cCyan_);

  drawPanel(128, 40, 100, 62);
  frame_.setTextSize(1);
  frame_.setTextColor(cMuted_);
  frame_.setCursor(138, 48);
  frame_.print("TARGET");
  drawTemperature(engine_.targetTemperatureC(), 178, 68, 2, cYellow_);

  drawRunGraph(12, 111, 216, 70);

  char elapsed[12];
  char total[12];
  formatTime(engine_.runElapsedMs(nowMs) / 1000UL, elapsed, sizeof(elapsed));
  formatTime(engine_.expectedDurationMs() / 1000UL, total, sizeof(total));
  char timeLine[28];
  snprintf(timeLine, sizeof(timeLine), "%s / %s", elapsed, total);
  frame_.setTextSize(1);
  frame_.setTextColor(cText_);
  frame_.setCursor(18, 190);
  frame_.print(timeLine);
  drawProgress(112, 188, 110, 13, engine_.progress(nowMs), cGreen_);

  drawButtons("STOP", paused ? "RESUME" : "PAUSE", "INFO");
}

void UiManager::drawRunInfo(uint32_t nowMs) {
  drawHeader("RUN INFO", engine_.stageName(), cYellow_);
  const ReflowProfile &profile = engine_.activeProfile();
  const ReflowStage &stage = profile.stages[engine_.stageIndex() < (profile.stageCount - 1U)
                            ? engine_.stageIndex()
                            : static_cast<uint8_t>(profile.stageCount - 1U)];

  drawPanel(12, 43, 216, 151);
  char line[48];
  frame_.setTextSize(1);
  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 54);
  frame_.print("Profile");
  drawFittedText(profile.name, 92, 49, 126, 18, 1, cText_,
                 TextAlign::LEFT);

  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 76);
  frame_.print("Stage mode");
  frame_.setTextColor(cText_);
  frame_.setCursor(92, 76);
  frame_.print(stageModeName(stage.mode));

  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 98);
  frame_.print("Heater");
  snprintf(line, sizeof(line), "%.0f%%", engine_.heaterDemandPercent());
  frame_.setTextColor(cOrange_);
  frame_.setCursor(92, 98);
  frame_.print(line);

  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 120);
  frame_.print("Peak");
  snprintf(line, sizeof(line), "%.1f C", engine_.peakTemperatureC());
  frame_.setTextColor(cText_);
  frame_.setCursor(92, 120);
  frame_.print(line);

  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 142);
  frame_.print("TAL actual");
  snprintf(line, sizeof(line), "%lus",
           static_cast<unsigned long>(engine_.timeAboveLiquidusMs() / 1000UL));
  frame_.setTextColor(cYellow_);
  frame_.setCursor(92, 142);
  frame_.print(line);

  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 164);
  frame_.print("Elapsed");
  formatTime(engine_.runElapsedMs(nowMs) / 1000UL, line, sizeof(line));
  frame_.setTextColor(cText_);
  frame_.setCursor(92, 164);
  frame_.print(line);

  drawButtons("BACK", engine_.state() == RunState::PAUSED ? "RESUME" : "PAUSE",
              "GRAPH");
}

void UiManager::drawComplete() {
  drawHeader("COMPLETE", "COOL", cGreen_);
  drawPanel(12, 45, 216, 96);
  drawCentered("OK", 58, 4, cGreen_);
  drawCentered("Reflow complete", 112, 2, cText_);

  char line[48];
  snprintf(line, sizeof(line), "Peak: %.1f C", engine_.peakTemperatureC());
  frame_.setTextSize(1);
  frame_.setTextColor(cText_);
  frame_.setCursor(23, 153);
  frame_.print(line);
  snprintf(line, sizeof(line), "Time above liquidus: %lus",
           static_cast<unsigned long>(engine_.timeAboveLiquidusMs() / 1000UL));
  frame_.setCursor(23, 172);
  frame_.print(line);
  frame_.setTextColor(cYellow_);
  frame_.setCursor(23, 192);
  frame_.print("Do not handle PCB while hot");
  drawButtons("HOME", "LOG", sensor_.valid() ? "REPEAT" : "LOCKED");
}

void UiManager::drawMenu() {
  drawHeader("MENU");
  static const char *items[] = {
      "Manual heat", "Calibration", "Logs", "Settings", "About"};
  constexpr uint8_t count = sizeof(items) / sizeof(items[0]);
  cursor_ = clampCursor(cursor_, count);
  for (uint8_t i = 0; i < count; ++i) {
    const int16_t y = 44 + i * 32;
    drawPanel(16, y, 208, 27, i == cursor_);
    frame_.setTextSize(1);
    frame_.setTextColor(cText_);
    frame_.setCursor(28, y + 9);
    frame_.print(items[i]);
  }
  drawButtons("BACK", "OPEN", "DOWN");
}

void UiManager::drawManual() {
  const bool active = engine_.state() == RunState::MANUAL;
  drawHeader("MANUAL HEAT", active ? "ON" : "OFF",
             active ? cOrange_ : cRed_);
  drawPanel(12, 45, 216, 83);
  drawTemperature(manualSetpointC_, 120, 60, 4, cOrange_);
  drawCentered("Setpoint", 106, 1, cMuted_);

  drawCentered("Actual", 139, 1, cMuted_);
  drawTemperature(sensor_.valid() ? sensor_.temperatureC() : NAN,
                  120, 151, 2, cText_);
  drawProgress(20, 182, 200, 13,
               active ? engine_.heaterDemandPercent() / 100.0f : 0.0f,
               cRed_);
  char line[48];
  snprintf(line, sizeof(line), "Heater output: %.0f%%",
           active ? engine_.heaterDemandPercent() : 0.0f);
  drawCentered(line, 199, 1, cMuted_);
  const bool canStartManual = sensor_.valid() &&
                              engine_.state() == RunState::IDLE &&
                              !autotuner_.active() && !ota_.active();
  drawButtons("-", active ? "OFF" : (canStartManual ? "ON" : "LOCKED"), "+");
}

void UiManager::drawCalibration() {
  drawHeader("CALIBRATION");
  drawPanel(12, 48, 216, 102, true, cCyan_);
  char value[24];
  snprintf(value, sizeof(value), "%+.1f C", calibrationWorkingC_);
  drawCentered(value, 72, 4, cCyan_);
  drawCentered("Temperature correction offset", 125, 1, cMuted_);

  frame_.setTextSize(1);
  frame_.setTextColor(cYellow_);
  frame_.setCursor(18, 169);
  frame_.print("Calibrate against a trusted probe");
  frame_.setTextColor(cMuted_);
  frame_.setCursor(18, 188);
  frame_.print("Adjustment step: 0.1 C");
  drawButtons("-", "SAVE", "+");
}

void UiManager::drawLogs() {
  drawHeader("RUN LOGS");
  const uint8_t count = profiles_.runLogCount();
  if (count == 0) {
    drawPanel(12, 50, 216, 116);
    drawCentered("No run logs", 88, 2, cMuted_);
    drawCentered("Logs appear after reflow", 122, 1, cMuted_);
  } else {
    logCursor_ = clampCursor(logCursor_, count);
    const RunSummary &log = profiles_.runLogNewest(logCursor_);
    drawPanel(12, 43, 216, 154);
    drawFittedText(log.profileName, 22, 49, 196, 28, 2, cText_,
                   TextAlign::LEFT);

    char line[48];
    snprintf(line, sizeof(line), "Peak %.1f C", log.peakTemperatureC);
    frame_.setTextSize(1);
    frame_.setTextColor(cCyan_);
    frame_.setCursor(22, 89);
    frame_.print(line);
    snprintf(line, sizeof(line), "TAL %us", log.timeAboveLiquidusS);
    frame_.setCursor(22, 111);
    frame_.print(line);
    snprintf(line, sizeof(line), "Total %us", log.totalTimeS);
    frame_.setCursor(22, 133);
    frame_.print(line);
    snprintf(line, sizeof(line), "Log %u of %u", logCursor_ + 1U, count);
    frame_.setTextColor(cMuted_);
    frame_.setCursor(22, 171);
    frame_.print(line);
  }
  drawButtons("BACK", "HOME", count > 1U ? "NEXT" : "NONE");
}

void UiManager::drawSettings() {
  drawHeader("SETTINGS");
  static const char *items[] = {
      "Button buzzer", "Fan during cool", "Backlight", "Idle dim",
      "Screen off", "Dim level", "Theme", "PID autotune",
      "OTA update", "Reset profiles", "About", "Back"};
  constexpr uint8_t count = sizeof(items) / sizeof(items[0]);
  cursor_ = clampCursor(cursor_, count);
  uint8_t first = 0;
  if (cursor_ >= VISIBLE_EDIT_ROWS) first = cursor_ - VISIBLE_EDIT_ROWS + 1U;

  for (uint8_t row = 0; row < VISIBLE_EDIT_ROWS; ++row) {
    const uint8_t i = first + row;
    if (i >= count) break;
    const int16_t y = 44 + row * 32;
    drawPanel(16, y, 208, 27, i == cursor_);
    char value[18] = "";
    if (i == 0) {
      strlcpy(value, profiles_.settings().buzzerEnabled ? "ON" : "OFF",
              sizeof(value));
    } else if (i == 1) {
      strlcpy(value, profiles_.settings().fanDuringCool ? "ON" : "OFF",
              sizeof(value));
    } else if (i == 2) {
      snprintf(value, sizeof(value), "%u%%",
               profiles_.settings().backlightPercent);
    } else if (i == 3) {
      formatIdleDimDelay(profiles_.settings().idleDimSeconds, value,
                         sizeof(value));
    } else if (i == 4) {
      formatIdleOffDelay(profiles_.settings().idleOffMinutes, value,
                         sizeof(value));
    } else if (i == 5) {
      snprintf(value, sizeof(value), "%u%%",
               profiles_.settings().idleDimPercent);
    } else if (i == 6) {
      strlcpy(value, themeName(static_cast<UiTheme>(
                         profiles_.settings().themeId)), sizeof(value));
    } else if (i == 7) {
      strlcpy(value, "OPEN", sizeof(value));
    } else if (i == 8) {
      strlcpy(value, ota_.active() ? "ACTIVE" : "OPEN", sizeof(value));
    } else if (i == 9) {
      strlcpy(value, "RESTORE", sizeof(value));
    } else if (i == 10) {
      strlcpy(value, "OPEN", sizeof(value));
    } else if (i == 11) {
      strlcpy(value, "DONE", sizeof(value));
    }

    const bool destructive = i == 9;
    const bool done = i == 11;
    drawFittedText(items[i], 28, y, 126, 27, 1, cText_, TextAlign::LEFT);
    drawFittedText(value, 160, y, 48, 27, 1,
                   destructive ? cRed_ : (done ? cGreen_ : cMuted_),
                   TextAlign::RIGHT);
  }
  drawScrollIndicator(count, first, VISIBLE_EDIT_ROWS);
  drawButtons("BACK", "CHANGE", "DOWN");
}

void UiManager::drawPidAutotune(uint32_t nowMs) {
  (void)nowMs;
  const bool active = autotuner_.active();
  const bool complete = autotuner_.complete();
  const bool failed = autotuner_.failed() ||
                      autotuner_.state() == PidAutotuner::State::ABORTED;
  drawHeader("PID AUTOTUNE", autotuner_.stateName(),
             failed ? cRed_ : (complete ? cGreen_ : cYellow_));

  if (!active && !complete && !failed) {
    drawPanel(12, 43, 216, 91, true, cOrange_);
    drawTemperature(autotuneTargetC_, 120, 59, 4, cOrange_);
    drawCentered("Autotune target", 112, 1, cMuted_);
    drawCentered("Empty oven; close door", 151, 1, cYellow_);
    drawCentered("Heater cycles at bounded power", 170, 1, cMuted_);
    drawCentered("Tune is not saved automatically", 189, 1, cMuted_);
    const bool canStart = sensor_.valid() &&
                          engine_.state() == RunState::IDLE && !ota_.active();
    drawButtons("-", canStart ? "START" : "LOCKED", "+");
    return;
  }

  if (complete) {
    drawPanel(12, 43, 216, 145, true, cGreen_);
    char line[40];
    drawCentered("Calculated PID", 54, 2, cGreen_);
    snprintf(line, sizeof(line), "Kp  %.3f", autotuner_.kp());
    drawCentered(line, 90, 2, cText_);
    snprintf(line, sizeof(line), "Ki  %.4f", autotuner_.ki());
    drawCentered(line, 116, 2, cText_);
    snprintf(line, sizeof(line), "Kd  %.2f", autotuner_.kd());
    drawCentered(line, 142, 2, cText_);
    snprintf(line, sizeof(line), "Tu %.1fs", autotuner_.ultimatePeriodS());
    drawCentered(line, 171, 1, cMuted_);
    drawButtons("BACK", "SAVE", "AGAIN");
    return;
  }

  if (failed) {
    drawPanel(12, 48, 216, 122, true, cRed_);
    drawCentered(autotuner_.stateName(), 61, 3, cRed_);
    drawWrappedText(autotuner_.detail(), 22, 108, 196, 2, 1, cText_,
                    TextAlign::CENTER);
    drawCentered("Heater is off", 142, 1, cYellow_);
    drawButtons("BACK", "RESET", "AGAIN");
    return;
  }

  drawPanel(12, 41, 100, 69);
  frame_.setTextSize(1);
  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 49);
  frame_.print("ACTUAL");
  drawTemperature(sensor_.valid() ? sensor_.temperatureC() : NAN,
                  62, 72, 2, cCyan_);

  drawPanel(128, 41, 100, 69);
  frame_.setTextSize(1);
  frame_.setTextColor(cMuted_);
  frame_.setCursor(138, 49);
  frame_.print("TARGET");
  drawTemperature(autotuner_.targetC(), 178, 72, 2, cYellow_);

  char line[48];
  snprintf(line, sizeof(line), "Cycles %u / %u",
           autotuner_.completedCycles(), autotuner_.requiredCycles());
  drawCentered(line, 124, 2, cText_);
  snprintf(line, sizeof(line), "Relay output %.0f%%",
           autotuner_.demandPercent());
  drawCentered(line, 153, 1, cOrange_);
  drawFittedText(autotuner_.detail(), 20, 170, 200, 15, 1, cMuted_,
                 TextAlign::CENTER);
  drawProgress(20, 194, 200, 10,
               static_cast<float>(autotuner_.completedCycles()) /
                   autotuner_.requiredCycles(),
               cGreen_);
  drawButtons("STOP", "DETAIL", "INFO");
}


void UiManager::drawPidAutotuneInfo(uint32_t nowMs) {
  (void)nowMs;
  const bool diagnostics = pidInfoView_ == PidInfoView::DIAGNOSTICS;
  drawHeader(diagnostics ? "TUNE DETAILS" : "TUNE INFO",
             autotuner_.stateName(),
             autotuner_.failed() ? cRed_ : cYellow_);

  drawPanel(12, 42, 216, 158, true,
            diagnostics ? cCyan_ : cPurple_);

  if (diagnostics) {
    char line[48];
    const TemperatureReading reading = sensor_.reading();

    frame_.setTextSize(1);
    frame_.setTextColor(cMuted_);
    frame_.setCursor(22, 53);
    frame_.print("Actual");
    snprintf(line, sizeof(line), reading.valid ? "%.1f C" : "INVALID",
             reading.filteredC);
    frame_.setTextColor(reading.valid ? cCyan_ : cRed_);
    frame_.setCursor(114, 53);
    frame_.print(line);

    frame_.setTextColor(cMuted_);
    frame_.setCursor(22, 76);
    frame_.print("Target");
    snprintf(line, sizeof(line), "%.1f C", autotuner_.targetC());
    frame_.setTextColor(cYellow_);
    frame_.setCursor(114, 76);
    frame_.print(line);

    frame_.setTextColor(cMuted_);
    frame_.setCursor(22, 99);
    frame_.print("Heater demand");
    snprintf(line, sizeof(line), "%.0f%%", autotuner_.demandPercent());
    frame_.setTextColor(cOrange_);
    frame_.setCursor(114, 99);
    frame_.print(line);

    frame_.setTextColor(cMuted_);
    frame_.setCursor(22, 122);
    frame_.print("Cycles");
    snprintf(line, sizeof(line), "%u / %u", autotuner_.completedCycles(),
             autotuner_.requiredCycles());
    frame_.setTextColor(cText_);
    frame_.setCursor(114, 122);
    frame_.print(line);

    frame_.setTextColor(cMuted_);
    frame_.setCursor(22, 145);
    frame_.print("Last peak");
    if (std::isfinite(autotuner_.latestPeakC())) {
      snprintf(line, sizeof(line), "%.1f C", autotuner_.latestPeakC());
    } else {
      strlcpy(line, "Waiting", sizeof(line));
    }
    frame_.setTextColor(cText_);
    frame_.setCursor(114, 145);
    frame_.print(line);

    frame_.setTextColor(cMuted_);
    frame_.setCursor(22, 168);
    frame_.print("Last trough");
    if (std::isfinite(autotuner_.latestTroughC())) {
      snprintf(line, sizeof(line), "%.1f C", autotuner_.latestTroughC());
    } else {
      strlcpy(line, "Waiting", sizeof(line));
    }
    frame_.setTextColor(cText_);
    frame_.setCursor(114, 168);
    frame_.print(line);
  } else {
    drawFittedText("Relay PID tuning", 22, 50, 196, 24, 2, cPurple_,
                   TextAlign::CENTER);
    drawCentered("The heater cycles around", 91, 1, cText_);
    drawCentered("the selected target.", 108, 1, cText_);
    drawCentered("Six cycles estimate Kp,", 132, 1, cMuted_);
    drawCentered("Ki and Kd for this oven.", 149, 1, cMuted_);
    drawCentered("STOP always forces heater off.", 176, 1, cYellow_);
  }

  drawButtons("BACK", autotuner_.active() ? "STOP" : "BACK", "NEXT");
}

void UiManager::drawOtaUpdate(uint32_t nowMs) {
  drawHeader("OTA UPDATE", ota_.stateName(),
             ota_.state() == OtaManager::State::ERROR ? cRed_ : cGreen_);

  if (!ota_.active()) {
    drawPanel(12, 46, 216, 154, true, cBlue_);
    drawFittedText("Browser update", 22, 57, 196, 24, 2, cCyan_,
                   TextAlign::CENTER);
    drawCentered("Wi-Fi is normally disabled", 101, 1, cMuted_);
    drawCentered("START creates a temporary AP", 122, 1, cMuted_);
    drawCentered("Heater remains forced off", 143, 1, cYellow_);
    char resetLine[40];
    snprintf(resetLine, sizeof(resetLine), "%s reset: %s",
             ota_.previousSessionInterrupted() ? "OTA" : "Last",
             ota_.bootResetReasonName());
    drawCentered(resetLine, 164, 1,
                 ota_.previousSessionInterrupted() ? cRed_ : cMuted_);
    drawCentered("Session closes after 10 minutes", 181, 1, cMuted_);
    const bool canStart = engine_.state() == RunState::IDLE &&
                          !autotuner_.active();
    drawButtons("BACK", canStart ? "START" : "LOCKED", "BACK");
    return;
  }

  drawPanel(12, 41, 216, 159);
  frame_.setTextSize(1);
  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 51);
  frame_.print("Wi-Fi");
  drawFittedText(ota_.ssid(), 78, 47, 140, 16, 1, cText_,
                 TextAlign::LEFT);

  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 75);
  frame_.print("Password");
  drawFittedText(ota_.password(), 78, 71, 140, 16, 1, cYellow_,
                 TextAlign::LEFT);

  frame_.setTextColor(cMuted_);
  frame_.setCursor(22, 99);
  frame_.print("Open");
  drawFittedText(ota_.address(), 78, 95, 140, 16, 1, cCyan_,
                 TextAlign::LEFT);

  if (ota_.uploading() || ota_.state() == OtaManager::State::SUCCESS) {
    drawProgress(22, 126, 196, 14, ota_.progressPercent() / 100.0f,
                 cGreen_);
    char progress[24];
    snprintf(progress, sizeof(progress), "%u%%", ota_.progressPercent());
    drawCentered(progress, 143, 2, cText_);
  } else {
    char remaining[32];
    snprintf(remaining, sizeof(remaining), "Closes in %lus",
             static_cast<unsigned long>(ota_.secondsRemaining(nowMs)));
    drawCentered(remaining, 131, 1, cMuted_);
  }
  char heapLine[40];
  if (ota_.freeHeapAfterWifi() > 0) {
    snprintf(heapLine, sizeof(heapLine), "Heap %luk (was %luk)",
             static_cast<unsigned long>(ota_.freeHeapAfterWifi() / 1024UL),
             static_cast<unsigned long>(ota_.freeHeapBeforeWifi() / 1024UL));
    drawCentered(heapLine, 164, 1, cMuted_);
  }
  drawFittedText(ota_.detail(), 20, 181, 200, 14, 1,
                 ota_.state() == OtaManager::State::ERROR ? cRed_ : cMuted_,
                 TextAlign::CENTER);
  if (ota_.uploading()) {
    drawButtons("LOCKED", "UPLOAD", "LOCKED");
  } else {
    drawButtons("STOP", "INFO", "STOP");
  }
}


void UiManager::drawOtaInfo(uint32_t nowMs) {
  (void)nowMs;
  drawHeader("OTA HELP", ota_.stateName(), cBlue_);
  drawPanel(12, 43, 216, 157, true, cBlue_);
  drawFittedText("Upload firmware", 22, 52, 196, 24, 2, cCyan_,
                 TextAlign::CENTER);
  drawCentered("Use the plain .ino.bin file", 91, 1, cText_);
  drawCentered("Do not upload ZIP, bootloader,", 112, 1, cMuted_);
  drawCentered("partitions or merged images.", 129, 1, cMuted_);
  drawCentered("The heater remains disabled", 157, 1, cYellow_);
  drawCentered("until the OTA session closes.", 174, 1, cYellow_);

  const bool canStop = ota_.active() && !ota_.uploading();
  drawButtons("BACK", canStop ? "STOP" : "BACK", "BACK");
}

void UiManager::drawAbout() {
  drawHeader("ABOUT");
  drawPanel(12, 44, 216, 157);
  drawCentered("Universal Reflow", 57, 2, cCyan_);
  drawCentered("Controller 1.9.3", 79, 2, cText_);

  drawFittedText("ESP32-S3-WROOM-1-N16", 22, 107, 196, 16, 1, cMuted_,
                 TextAlign::LEFT);
#if USE_NTC_100K_SENSOR
  drawFittedText("ST7789 240x240 + 100k NTC", 22, 126, 196, 16, 1,
                 cMuted_, TextAlign::LEFT);
#else
  drawFittedText("ST7789 240x240 + MAX31865", 22, 126, 196, 16, 1,
                 cMuted_, TextAlign::LEFT);
#endif
  drawFittedText("Profiles stored in NVS flash", 22, 145, 196, 16, 1,
                 cMuted_, TextAlign::LEFT);
  drawFittedText("Use a thermal fuse and enclosure", 22, 172, 196, 16, 1,
                 cYellow_, TextAlign::LEFT);
  drawButtons("BACK", "HOME", "BACK");
}

void UiManager::drawFault() {
  drawHeader("FAULT", "STOP", cRed_);
  frame_.fillRoundRect(12, 45, 216, 129, 14, CslessST7789::color565(45, 20, 26));
  frame_.drawRoundRect(12, 45, 216, 129, 14, cRed_);
  drawCentered("!", 55, 6, cRed_);
  drawCentered(faultCodeName(engine_.faultCode()), 117, 1, cText_);
  drawWrappedText(engine_.faultDetail(), 22, 136, 196, 2, 1, cMuted_,
                  TextAlign::CENTER);

  drawFittedText("Hold RESET to clear fault", 22, 183, 196, 16, 1,
                 cYellow_, TextAlign::LEFT);
  drawButtons("HOME", "DETAIL", "HOLD RST");
}


void UiManager::drawFaultDetail() {
  drawHeader("FAULT INFO", "HEATER OFF", cRed_);
  drawPanel(12, 42, 216, 159, true, cRed_);

  char line[48];
  const TemperatureReading reading = sensor_.reading();

  drawFittedText("Fault", 22, 49, 54, 16, 1, cMuted_, TextAlign::LEFT);
  drawFittedText(faultCodeName(engine_.faultCode()), 82, 49, 136, 16, 1,
                 cRed_, TextAlign::LEFT);

  drawFittedText("Detail", 22, 72, 54, 16, 1, cMuted_, TextAlign::LEFT);
  drawWrappedText(engine_.faultDetail(), 22, 89, 196, 2, 1, cText_,
                  TextAlign::LEFT, 2);

  drawFittedText("Sensor", 22, 119, 84, 16, 1, cMuted_, TextAlign::LEFT);
  drawFittedText(reading.valid ? "VALID" : "INVALID", 112, 119, 106, 16, 1,
                 reading.valid ? cGreen_ : cRed_, TextAlign::LEFT);

  drawFittedText("Temperature", 22, 142, 84, 16, 1, cMuted_,
                 TextAlign::LEFT);
  if (reading.valid) {
    snprintf(line, sizeof(line), "%.1f C", reading.filteredC);
  } else {
    strlcpy(line, "--.- C", sizeof(line));
  }
  drawFittedText(line, 112, 142, 106, 16, 1, cText_, TextAlign::LEFT);

  drawFittedText("Heater output", 22, 165, 84, 16, 1, cMuted_,
                 TextAlign::LEFT);
  drawFittedText(heater_.outputOn() ? "ON" : "OFF", 112, 165, 106, 16, 1,
                 heater_.outputOn() ? cRed_ : cGreen_, TextAlign::LEFT);

  drawFittedText("Fix cause before reset", 22, 187, 196, 11, 1, cYellow_,
                 TextAlign::CENTER);
  drawButtons("BACK", "HOME", "HOLD RST");
}

void UiManager::drawDeleteConfirm() {
  drawHeader("DELETE", "CONFIRM", cRed_);
  drawPanel(12, 51, 216, 116, true, cRed_);
  drawFittedText("Delete profile?", 22, 66, 196, 24, 2, cText_,
                 TextAlign::CENTER);
  drawFittedText(editProfile_.name, 22, 99, 196, 24, 2, cRed_,
                 TextAlign::CENTER);
  drawCentered("This cannot be undone", 145, 1, cYellow_);
  drawButtons("CANCEL", "DELETE", "CANCEL");
}

void UiManager::handleHome(const ButtonEvent &event, uint32_t nowMs) {
  if (isPress(event, ButtonId::LEFT)) {
    cursor_ = profiles_.selectedIndex();
    page_ = Page::PROFILE_LIST;
  } else if (isPress(event, ButtonId::MIDDLE) && sensor_.valid() &&
             engine_.state() == RunState::IDLE && !autotuner_.active() &&
             !ota_.active()) {
    startSelectedProfile(nowMs);
  } else if (isPress(event, ButtonId::RIGHT)) {
    cursor_ = 0;
    page_ = Page::MENU;
  }
}

void UiManager::handleProfileList(const ButtonEvent &event) {
  const bool canAddProfile = profiles_.profileCount() < MAX_PROFILES;
  const uint8_t total = profiles_.profileCount() +
                        (canAddProfile ? 1U : 0U);
  if (isPress(event, ButtonId::LEFT)) {
    page_ = Page::HOME;
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    cursor_ = static_cast<uint8_t>((cursor_ + 1U) % total);
  } else if (isPress(event, ButtonId::MIDDLE)) {
    if (canAddProfile && cursor_ == profiles_.profileCount()) {
      const int8_t created = profiles_.addDuplicate(profiles_.selectedIndex());
      if (created >= 0) {
        editProfileIndex_ = static_cast<uint8_t>(created);
        beginProfileEdit();
      }
    } else {
      profiles_.selectProfile(cursor_);
      page_ = Page::PROFILE_DETAIL;
    }
  }
}

void UiManager::handleProfileDetail(const ButtonEvent &event,
                                    uint32_t nowMs) {
  if (isPress(event, ButtonId::LEFT)) {
    cursor_ = profiles_.selectedIndex();
    page_ = Page::PROFILE_LIST;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    editProfileIndex_ = profiles_.selectedIndex();
    beginProfileEdit();
  } else if (isPress(event, ButtonId::RIGHT) && sensor_.valid() &&
             engine_.state() == RunState::IDLE && !autotuner_.active() &&
             !ota_.active()) {
    startSelectedProfile(nowMs);
  }
}

void UiManager::handleProfileEdit(const ButtonEvent &event) {
  constexpr uint8_t itemCount = 8;
  if (isPress(event, ButtonId::LEFT)) {
    page_ = Page::PROFILE_DETAIL;
    return;
  }
  if (isAdjust(event, ButtonId::RIGHT)) {
    cursor_ = static_cast<uint8_t>((cursor_ + 1U) % itemCount);
    return;
  }
  if (!isPress(event, ButtonId::MIDDLE)) return;

  switch (cursor_) {
    case 0: beginNameEdit(NameKind::PROFILE, Page::PROFILE_EDIT); break;
    case 1: beginValueEdit(ValueKind::LIQUIDUS, Page::PROFILE_EDIT); break;
    case 2: beginValueEdit(ValueKind::MAX_TEMPERATURE, Page::PROFILE_EDIT); break;
    case 3: beginValueEdit(ValueKind::MAX_RAMP, Page::PROFILE_EDIT); break;
    case 4: beginValueEdit(ValueKind::TAL_TARGET, Page::PROFILE_EDIT); break;
    case 5:
      cursor_ = 0;
      page_ = Page::STAGE_LIST;
      break;
    case 6:
      if (profiles_.profileCount() > 1) page_ = Page::DELETE_CONFIRM;
      break;
    case 7:
      saveProfileEdit();
      break;
  }
}

void UiManager::handleStageList(const ButtonEvent &event) {
  const uint8_t total = editProfile_.stageCount +
                        (editProfile_.stageCount < MAX_PROFILE_STAGES ? 1U : 0U);
  if (isPress(event, ButtonId::LEFT)) {
    cursor_ = 5;
    page_ = Page::PROFILE_EDIT;
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    cursor_ = static_cast<uint8_t>((cursor_ + 1U) % total);
  } else if (isPress(event, ButtonId::MIDDLE)) {
    if (cursor_ == editProfile_.stageCount) {
      addStage();
      editStageIndex_ = editProfile_.stageCount - 1U;
    } else {
      editStageIndex_ = cursor_;
    }
    cursor_ = 0;
    page_ = Page::STAGE_EDIT;
  }
}

void UiManager::handleStageEdit(const ButtonEvent &event) {
  constexpr uint8_t itemCount = 8;
  if (isPress(event, ButtonId::LEFT)) {
    cursor_ = editStageIndex_;
    page_ = Page::STAGE_LIST;
    return;
  }
  if (isAdjust(event, ButtonId::RIGHT)) {
    cursor_ = static_cast<uint8_t>((cursor_ + 1U) % itemCount);
    return;
  }
  if (!isPress(event, ButtonId::MIDDLE)) return;

  ReflowStage &stage = editProfile_.stages[editStageIndex_];
  switch (cursor_) {
    case 0: beginNameEdit(NameKind::STAGE, Page::STAGE_EDIT); break;
    case 1:
      stage.mode = static_cast<StageMode>((static_cast<uint8_t>(stage.mode) + 1U) % 3U);
      break;
    case 2: beginValueEdit(ValueKind::STAGE_TARGET, Page::STAGE_EDIT); break;
    case 3: beginValueEdit(ValueKind::STAGE_DURATION, Page::STAGE_EDIT); break;
    case 4: moveStage(-1); break;
    case 5: moveStage(1); break;
    case 6:
      deleteStage();
      cursor_ = editStageIndex_ < (editProfile_.stageCount - 1U)
                    ? editStageIndex_
                    : static_cast<uint8_t>(editProfile_.stageCount - 1U);
      page_ = Page::STAGE_LIST;
      break;
    case 7:
      cursor_ = editStageIndex_;
      page_ = Page::STAGE_LIST;
      break;
  }
}

void UiManager::handleValueEdit(const ButtonEvent &event) {
  if (isAdjust(event, ButtonId::LEFT)) {
    adjustValue(-1);
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    adjustValue(1);
  } else if (isPress(event, ButtonId::MIDDLE)) {
    page_ = valueReturnPage_;
  }
}

void UiManager::handleNameEdit(const ButtonEvent &event) {
  if (isAdjust(event, ButtonId::LEFT)) {
    cycleNameCharacter(-1);
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    cycleNameCharacter(1);
  } else if (event.button == ButtonId::MIDDLE &&
             event.action == ButtonAction::SHORT_PRESS) {
    const size_t capacity = activeNameCapacity();
    if (nameCursor_ + 1U < capacity - 1U) {
      ++nameCursor_;
      char *buffer = activeNameBuffer();
      if (buffer[nameCursor_] == '\0') {
        buffer[nameCursor_] = ' ';
        buffer[nameCursor_ + 1U] = '\0';
      }
    } else {
      char *buffer = activeNameBuffer();
      int end = static_cast<int>(strlen(buffer)) - 1;
      while (end > 0 && buffer[end] == ' ') {
        buffer[end--] = '\0';
      }
      if (buffer[0] == '\0') {
        strlcpy(buffer, "Profile", activeNameCapacity());
      }
      page_ = nameReturnPage_;
    }
  } else if (event.button == ButtonId::MIDDLE &&
             event.action == ButtonAction::LONG_PRESS) {
    char *buffer = activeNameBuffer();
    // Trim trailing spaces while preserving at least one character.
    int end = static_cast<int>(strlen(buffer)) - 1;
    while (end > 0 && buffer[end] == ' ') {
      buffer[end--] = '\0';
    }
    if (buffer[0] == '\0') strlcpy(buffer, "Profile", activeNameCapacity());
    page_ = nameReturnPage_;
  }
}

void UiManager::handleRunning(const ButtonEvent &event, uint32_t nowMs) {
  if (isPress(event, ButtonId::LEFT)) {
    engine_.abortRun();
    page_ = Page::HOME;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    engine_.pauseOrResume(nowMs);
  } else if (isPress(event, ButtonId::RIGHT)) {
    page_ = Page::RUN_INFO;
  }
}

void UiManager::handleRunInfo(const ButtonEvent &event, uint32_t nowMs) {
  if (isPress(event, ButtonId::LEFT) || isPress(event, ButtonId::RIGHT)) {
    page_ = Page::RUNNING;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    engine_.pauseOrResume(nowMs);
  }
}

void UiManager::handleComplete(const ButtonEvent &event, uint32_t nowMs) {
  if (isPress(event, ButtonId::LEFT)) {
    engine_.abortRun();
    page_ = Page::HOME;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    logCursor_ = 0;
    page_ = Page::LOGS;
  } else if (isPress(event, ButtonId::RIGHT)) {
    const ReflowProfile repeat = engine_.activeProfile();
    engine_.abortRun();
    if (sensor_.valid()) {
      engine_.startProfile(repeat, sensor_.temperatureC(), nowMs);
    }
  }
}

void UiManager::handleMenu(const ButtonEvent &event) {
  constexpr uint8_t itemCount = 5;
  if (isPress(event, ButtonId::LEFT)) {
    page_ = Page::HOME;
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    cursor_ = static_cast<uint8_t>((cursor_ + 1U) % itemCount);
  } else if (isPress(event, ButtonId::MIDDLE)) {
    switch (cursor_) {
      case 0:
        manualSetpointC_ = 120.0f;
        page_ = Page::MANUAL;
        break;
      case 1:
        calibrationWorkingC_ = profiles_.settings().temperatureOffsetC;
        page_ = Page::CALIBRATION;
        break;
      case 2:
        logCursor_ = 0;
        page_ = Page::LOGS;
        break;
      case 3:
        cursor_ = 0;
        page_ = Page::SETTINGS;
        break;
      case 4:
        page_ = Page::ABOUT;
        break;
    }
  }
}

void UiManager::handleManual(const ButtonEvent &event, uint32_t nowMs) {
  if (isAdjust(event, ButtonId::LEFT)) {
    manualSetpointC_ = max(40.0f, manualSetpointC_ - 5.0f);
    if (engine_.state() == RunState::MANUAL) engine_.setManualTarget(manualSetpointC_);
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    manualSetpointC_ = min(250.0f, manualSetpointC_ + 5.0f);
    if (engine_.state() == RunState::MANUAL) engine_.setManualTarget(manualSetpointC_);
  } else if (event.button == ButtonId::MIDDLE &&
             event.action == ButtonAction::SHORT_PRESS) {
    if (engine_.state() == RunState::MANUAL) {
      engine_.abortRun();
    } else if (sensor_.valid() && engine_.state() == RunState::IDLE &&
               !autotuner_.active() && !ota_.active()) {
      engine_.startManual(manualSetpointC_, sensor_.temperatureC(), nowMs);
    }
  } else if (event.button == ButtonId::MIDDLE &&
             event.action == ButtonAction::LONG_PRESS) {
    engine_.abortRun();
    cursor_ = 0;
    page_ = Page::MENU;
  }
}

void UiManager::handleCalibration(const ButtonEvent &event) {
  if (isAdjust(event, ButtonId::LEFT)) {
    calibrationWorkingC_ = max(-20.0f, calibrationWorkingC_ - 0.1f);
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    calibrationWorkingC_ = min(20.0f, calibrationWorkingC_ + 0.1f);
  } else if (isPress(event, ButtonId::MIDDLE)) {
    profiles_.settings().temperatureOffsetC = calibrationWorkingC_;
    profiles_.save();
    sensor_.setCalibrationOffset(calibrationWorkingC_);
    cursor_ = 1;
    page_ = Page::MENU;
  }
}

void UiManager::handleLogs(const ButtonEvent &event) {
  const uint8_t count = profiles_.runLogCount();
  if (isPress(event, ButtonId::LEFT)) {
    cursor_ = 2;
    page_ = Page::MENU;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    engine_.abortRun();
    page_ = Page::HOME;
  } else if (isAdjust(event, ButtonId::RIGHT) && count > 0) {
    logCursor_ = static_cast<uint8_t>((logCursor_ + 1U) % count);
  }
}

void UiManager::handleSettings(const ButtonEvent &event) {
  constexpr uint8_t itemCount = 12;
  if (isPress(event, ButtonId::LEFT)) {
    cursor_ = 3;
    page_ = Page::MENU;
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    cursor_ = static_cast<uint8_t>((cursor_ + 1U) % itemCount);
  } else if (isPress(event, ButtonId::MIDDLE)) {
    switch (cursor_) {
      case 0:
        profiles_.settings().buzzerEnabled =
            !profiles_.settings().buzzerEnabled;
        profiles_.save();
        break;
      case 1:
        profiles_.settings().fanDuringCool =
            !profiles_.settings().fanDuringCool;
        profiles_.save();
        break;
      case 2: {
        uint8_t brightness = profiles_.settings().backlightPercent;
        brightness =
            static_cast<uint8_t>(brightness + TFT_BACKLIGHT_STEP_PERCENT);
        if (brightness > 100U) brightness = TFT_BACKLIGHT_MIN_PERCENT;
        profiles_.settings().backlightPercent = brightness;
        restoreConfiguredBacklight();
        profiles_.save();
        break;
      }
      case 3:
        profiles_.settings().idleDimSeconds =
            nextIdleDimSeconds(profiles_.settings().idleDimSeconds);
        profiles_.save();
        break;
      case 4:
        profiles_.settings().idleOffMinutes =
            nextIdleOffMinutes(profiles_.settings().idleOffMinutes);
        profiles_.save();
        break;
      case 5:
        profiles_.settings().idleDimPercent =
            nextIdleDimPercent(profiles_.settings().idleDimPercent);
        profiles_.save();
        break;
      case 6: {
        uint8_t next = static_cast<uint8_t>(profiles_.settings().themeId + 1U);
        if (next >= static_cast<uint8_t>(UiTheme::COUNT)) next = 0;
        profiles_.settings().themeId = next;
        profiles_.save();
        applyTheme();
        break;
      }
      case 7:
        autotuneTargetC_ = PID_AUTOTUNE_DEFAULT_TARGET_C;
        autotuner_.reset();
        page_ = Page::PID_AUTOTUNE;
        break;
      case 8:
        page_ = Page::OTA_UPDATE;
        break;
      case 9:
        profiles_.resetDefaults();
        profiles_.save();
        sensor_.setCalibrationOffset(profiles_.settings().temperatureOffsetC);
        heater_.setPidTunings(profiles_.settings().pidKp,
                              profiles_.settings().pidKi,
                              profiles_.settings().pidKd);
        applyTheme();
        restoreConfiguredBacklight();
        lastInteractionMs_ = millis();
        break;
      case 10:
        page_ = Page::ABOUT;
        break;
      case 11:
        cursor_ = 3;
        page_ = Page::MENU;
        break;
    }
  }
}

void UiManager::handlePidAutotune(const ButtonEvent &event,
                                  uint32_t nowMs) {
  if (autotuner_.active()) {
    if (isPress(event, ButtonId::LEFT)) {
      autotuner_.abort("Stopped by user");
    } else if (isPress(event, ButtonId::MIDDLE)) {
      pidInfoView_ = PidInfoView::DIAGNOSTICS;
      page_ = Page::PID_AUTOTUNE_INFO;
    } else if (isPress(event, ButtonId::RIGHT)) {
      pidInfoView_ = PidInfoView::HELP;
      page_ = Page::PID_AUTOTUNE_INFO;
    }
    return;
  }

  if (autotuner_.complete()) {
    if (isPress(event, ButtonId::LEFT)) {
      autotuner_.reset();
      cursor_ = 7;
      page_ = Page::SETTINGS;
    } else if (isPress(event, ButtonId::MIDDLE)) {
      profiles_.settings().pidKp = autotuner_.kp();
      profiles_.settings().pidKi = autotuner_.ki();
      profiles_.settings().pidKd = autotuner_.kd();
      profiles_.save();
      heater_.setPidTunings(autotuner_.kp(), autotuner_.ki(),
                            autotuner_.kd());
      autotuner_.reset();
      cursor_ = 7;
      page_ = Page::SETTINGS;
    } else if (isPress(event, ButtonId::RIGHT)) {
      autotuner_.reset();
    }
    return;
  }

  if (autotuner_.failed() ||
      autotuner_.state() == PidAutotuner::State::ABORTED) {
    if (isPress(event, ButtonId::LEFT)) {
      autotuner_.reset();
      cursor_ = 7;
      page_ = Page::SETTINGS;
    } else if (isPress(event, ButtonId::MIDDLE) ||
               isPress(event, ButtonId::RIGHT)) {
      autotuner_.reset();
    }
    return;
  }

  if (isAdjust(event, ButtonId::LEFT)) {
    autotuneTargetC_ = max(PID_AUTOTUNE_MIN_TARGET_C,
                           autotuneTargetC_ - PID_AUTOTUNE_TARGET_STEP_C);
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    autotuneTargetC_ = min(PID_AUTOTUNE_MAX_TARGET_C,
                           autotuneTargetC_ + PID_AUTOTUNE_TARGET_STEP_C);
  } else if (isPress(event, ButtonId::MIDDLE) && sensor_.valid() &&
             engine_.state() == RunState::IDLE && !ota_.active()) {
    autotuner_.start(autotuneTargetC_, sensor_.reading(), nowMs);
  }
}


void UiManager::handlePidAutotuneInfo(const ButtonEvent &event) {
  if (isPress(event, ButtonId::LEFT)) {
    page_ = Page::PID_AUTOTUNE;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    if (autotuner_.active()) {
      autotuner_.abort("Stopped from info page");
    }
    page_ = Page::PID_AUTOTUNE;
  } else if (isPress(event, ButtonId::RIGHT)) {
    pidInfoView_ = pidInfoView_ == PidInfoView::DIAGNOSTICS
                       ? PidInfoView::HELP
                       : PidInfoView::DIAGNOSTICS;
  }
}

void UiManager::handleOtaUpdate(const ButtonEvent &event, uint32_t nowMs) {
  if (!ota_.active()) {
    if (isPress(event, ButtonId::LEFT) || isPress(event, ButtonId::RIGHT)) {
      cursor_ = 8;
      page_ = Page::SETTINGS;
    } else if (isPress(event, ButtonId::MIDDLE) &&
               engine_.state() == RunState::IDLE && !autotuner_.active()) {
      engine_.abortRun();
      heater_.forceOff();
      // Reduce display load before starting the radio. On a marginal 3.3 V
      // rail, waiting until after softAP() succeeds is too late to reduce the
      // Wi-Fi startup current transient.
      const uint8_t otaBrightness =
          min(profiles_.settings().backlightPercent,
              static_cast<uint8_t>(OTA_BACKLIGHT_PERCENT));
      backlight_.setPercent(otaBrightness);
      backlightState_ = BacklightState::ACTIVE;
      if (ota_.start(nowMs)) {
        lastOtaActive_ = true;
      } else {
        restoreConfiguredBacklight();
      }
    }
    return;
  }

  // Uploading is intentionally not interruptible from the buttons because
  // aborting a flash write is more hazardous than allowing it to finish.
  if (ota_.uploading()) {
    return;
  }

  if (isPress(event, ButtonId::LEFT) || isPress(event, ButtonId::RIGHT)) {
    ota_.stop();
    restoreConfiguredBacklight();
    lastOtaActive_ = false;
    cursor_ = 8;
    page_ = Page::SETTINGS;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    page_ = Page::OTA_INFO;
  }
}

void UiManager::handleOtaInfo(const ButtonEvent &event) {
  if (isPress(event, ButtonId::LEFT) || isPress(event, ButtonId::RIGHT)) {
    page_ = Page::OTA_UPDATE;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    if (ota_.active() && !ota_.uploading()) {
      ota_.stop();
      restoreConfiguredBacklight();
      lastOtaActive_ = false;
      cursor_ = 8;
      page_ = Page::SETTINGS;
    } else {
      page_ = Page::OTA_UPDATE;
    }
  }
}

void UiManager::handleAbout(const ButtonEvent &event) {
  if (isPress(event, ButtonId::MIDDLE)) {
    page_ = Page::HOME;
  } else if (isPress(event, ButtonId::LEFT) || isPress(event, ButtonId::RIGHT)) {
    cursor_ = 4;
    page_ = Page::MENU;
  }
}

void UiManager::handleFault(const ButtonEvent &event) {
  if (isPress(event, ButtonId::LEFT)) {
    page_ = Page::HOME;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    page_ = Page::FAULT_DETAIL;
  } else if (event.button == ButtonId::RIGHT &&
             event.action == ButtonAction::LONG_PRESS &&
             engine_.clearFault()) {
    page_ = Page::HOME;
  }
}

void UiManager::handleFaultDetail(const ButtonEvent &event) {
  if (isPress(event, ButtonId::LEFT)) {
    page_ = Page::FAULT;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    page_ = Page::HOME;
  } else if (event.button == ButtonId::RIGHT &&
             event.action == ButtonAction::LONG_PRESS &&
             engine_.clearFault()) {
    page_ = Page::HOME;
  }
}

void UiManager::handleDeleteConfirm(const ButtonEvent &event) {
  if (isPress(event, ButtonId::LEFT) || isPress(event, ButtonId::RIGHT)) {
    page_ = Page::PROFILE_EDIT;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    profiles_.deleteProfile(editProfileIndex_);
    cursor_ = profiles_.selectedIndex();
    page_ = Page::PROFILE_LIST;
  }
}

void UiManager::beginProfileEdit() {
  editProfile_ = profiles_.profile(editProfileIndex_);
  cursor_ = 0;
  page_ = Page::PROFILE_EDIT;
}

void UiManager::saveProfileEdit() {
  if (profiles_.updateProfile(editProfileIndex_, editProfile_)) {
    profiles_.selectProfile(editProfileIndex_);
    page_ = Page::PROFILE_DETAIL;
  }
}

void UiManager::beginValueEdit(ValueKind kind, Page returnPage) {
  valueKind_ = kind;
  valueReturnPage_ = returnPage;
  page_ = Page::VALUE_EDIT;
}

void UiManager::adjustValue(int direction) {
  ReflowStage &stage = editProfile_.stages[editStageIndex_];
  switch (valueKind_) {
    case ValueKind::LIQUIDUS:
      editProfile_.liquidusC = constrain(editProfile_.liquidusC + direction,
                                         60.0f,
                                         editProfile_.maxTemperatureC - 5.0f);
      break;
    case ValueKind::MAX_TEMPERATURE:
      editProfile_.maxTemperatureC = constrain(
          editProfile_.maxTemperatureC + direction,
          editProfile_.liquidusC + 5.0f, GLOBAL_MAX_TEMPERATURE_C);
      for (uint8_t i = 0; i < editProfile_.stageCount; ++i) {
        editProfile_.stages[i].targetC = min(
            editProfile_.stages[i].targetC, editProfile_.maxTemperatureC);
      }
      break;
    case ValueKind::MAX_RAMP:
      editProfile_.maxRampCPerSecond = constrain(
          editProfile_.maxRampCPerSecond + direction * 0.1f, 0.2f, 5.0f);
      break;
    case ValueKind::TAL_TARGET: {
      const int next = static_cast<int>(editProfile_.targetTimeAboveLiquidusS) +
                       direction * 5;
      editProfile_.targetTimeAboveLiquidusS =
          static_cast<uint16_t>(constrain(next, 10, 180));
      break;
    }
    case ValueKind::STAGE_TARGET:
      stage.targetC = constrain(stage.targetC + direction, 20.0f,
                                editProfile_.maxTemperatureC);
      break;
    case ValueKind::STAGE_DURATION: {
      const int next = static_cast<int>(stage.durationS) + direction * 5;
      stage.durationS = static_cast<uint16_t>(constrain(next, 5, 900));
      break;
    }
  }
}

void UiManager::beginNameEdit(NameKind kind, Page returnPage) {
  nameKind_ = kind;
  nameReturnPage_ = returnPage;
  nameCursor_ = 0;
  page_ = Page::NAME_EDIT;
}

char *UiManager::activeNameBuffer() {
  if (nameKind_ == NameKind::PROFILE) {
    return editProfile_.name;
  }
  return editProfile_.stages[editStageIndex_].name;
}

size_t UiManager::activeNameCapacity() const {
  if (nameKind_ == NameKind::PROFILE) {
    return sizeof(editProfile_.name);
  }
  return sizeof(editProfile_.stages[editStageIndex_].name);
}

void UiManager::cycleNameCharacter(int direction) {
  char *buffer = activeNameBuffer();
  const size_t capacity = activeNameCapacity();
  if (nameCursor_ >= capacity - 1U) return;

  if (buffer[nameCursor_] == '\0') {
    buffer[nameCursor_] = ' ';
    buffer[nameCursor_ + 1U] = '\0';
  }

  const char *position = strchr(NAME_CHARSET, buffer[nameCursor_]);
  int index = position ? static_cast<int>(position - NAME_CHARSET) : 0;
  const int count = static_cast<int>(strlen(NAME_CHARSET));
  index = (index + direction + count) % count;
  buffer[nameCursor_] = NAME_CHARSET[index];
}

void UiManager::addStage() {
  if (editProfile_.stageCount >= MAX_PROFILE_STAGES) return;
  ReflowStage &stage = editProfile_.stages[editProfile_.stageCount];
  stage = ReflowStage{};
  strlcpy(stage.name, "New stage", sizeof(stage.name));
  stage.mode = StageMode::RAMP;
  const float previous = editProfile_.stageCount == 0
                             ? 100.0f
                             : editProfile_.stages[editProfile_.stageCount - 1U]
                                   .targetC;
  stage.targetC = min(previous + 20.0f, editProfile_.maxTemperatureC);
  stage.durationS = 60;
  ++editProfile_.stageCount;
}

void UiManager::deleteStage() {
  if (editProfile_.stageCount <= 1 ||
      editStageIndex_ >= editProfile_.stageCount) {
    return;
  }
  for (uint8_t i = editStageIndex_; i + 1U < editProfile_.stageCount; ++i) {
    editProfile_.stages[i] = editProfile_.stages[i + 1U];
  }
  editProfile_.stages[editProfile_.stageCount - 1U] = ReflowStage{};
  --editProfile_.stageCount;
  if (editStageIndex_ >= editProfile_.stageCount) {
    editStageIndex_ = editProfile_.stageCount - 1U;
  }
}

void UiManager::moveStage(int direction) {
  const int next = static_cast<int>(editStageIndex_) + direction;
  if (next < 0 || next >= editProfile_.stageCount) return;
  ReflowStage temporary = editProfile_.stages[editStageIndex_];
  editProfile_.stages[editStageIndex_] = editProfile_.stages[next];
  editProfile_.stages[next] = temporary;
  editStageIndex_ = static_cast<uint8_t>(next);
}

bool UiManager::startSelectedProfile(uint32_t nowMs) {
  if (!sensor_.valid()) {
    engine_.triggerFault(FaultCode::SENSOR, sensor_.faultDescription());
    return false;
  }
  if (!profiles_.validateProfile(profiles_.selectedProfile())) {
    engine_.triggerFault(FaultCode::INVALID_PROFILE,
                         "Profile validation failed");
    return false;
  }
  return engine_.startProfile(profiles_.selectedProfile(),
                              sensor_.temperatureC(), nowMs);
}

void UiManager::beep(uint16_t durationMs) {
  if (PIN_BUZZER < 0 || !profiles_.settings().buzzerEnabled) return;
  digitalWrite(PIN_BUZZER, buzzerOnLevel());
  buzzerOffMs_ = millis() + durationMs;
}

bool UiManager::isPress(const ButtonEvent &event, ButtonId id) {
  return event.button == id &&
         (event.action == ButtonAction::SHORT_PRESS ||
          event.action == ButtonAction::LONG_PRESS);
}

bool UiManager::isAdjust(const ButtonEvent &event, ButtonId id) {
  return event.button == id &&
         (event.action == ButtonAction::SHORT_PRESS ||
          event.action == ButtonAction::LONG_PRESS ||
          event.action == ButtonAction::REPEAT);
}

#include "UiManager.h"

#include <cmath>
#include <cstring>

#include "Config.h"
#include "Safety.h"

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
}  // namespace

UiManager::UiManager(Adafruit_ST7789 &display, ProfileStore &profiles,
                     ReflowEngine &engine, TemperatureSensor &sensor,
                     BacklightController &backlight)
    : tft_(display), profiles_(profiles), engine_(engine), sensor_(sensor),
      backlight_(backlight) {}

void UiManager::begin() {
  cBg_ = tft_.color565(10, 12, 18);
  cPanel_ = tft_.color565(22, 27, 38);
  cPanel2_ = tft_.color565(31, 37, 52);
  cLine_ = tft_.color565(65, 76, 96);
  cText_ = tft_.color565(235, 241, 250);
  cMuted_ = tft_.color565(155, 166, 184);
  cCyan_ = tft_.color565(74, 211, 238);
  cGreen_ = tft_.color565(78, 220, 151);
  cYellow_ = tft_.color565(245, 198, 76);
  cOrange_ = tft_.color565(255, 143, 79);
  cRed_ = tft_.color565(245, 92, 96);
  cPurple_ = tft_.color565(170, 130, 255);
  cBlue_ = tft_.color565(92, 145, 255);

  if (PIN_BUZZER >= 0) {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, buzzerOffLevel());
  }

  calibrationWorkingC_ = profiles_.settings().temperatureOffsetC;
  manualSetpointC_ = 120.0f;
  cursor_ = profiles_.selectedIndex();
  dirty_ = true;
}

void UiManager::update(uint32_t nowMs) {
  if (PIN_BUZZER >= 0 && buzzerOffMs_ != 0 &&
      static_cast<int32_t>(nowMs - buzzerOffMs_) >= 0) {
    digitalWrite(PIN_BUZZER, buzzerOffLevel());
    buzzerOffMs_ = 0;
  }

  syncPageToRunState();

  const bool dynamicPage = page_ == Page::HOME || page_ == Page::RUNNING ||
                           page_ == Page::RUN_INFO || page_ == Page::MANUAL ||
                           page_ == Page::COMPLETE || page_ == Page::FAULT;
  if (!dirty_ && (!dynamicPage ||
                  (nowMs - lastDrawMs_) < UI_REFRESH_INTERVAL_MS)) {
    return;
  }

  drawCurrentPage(nowMs);
  dirty_ = false;
  lastDrawMs_ = nowMs;
}

void UiManager::handleButton(const ButtonEvent &event, uint32_t nowMs) {
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
    case Page::ABOUT: handleAbout(event); break;
    case Page::FAULT: handleFault(event); break;
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

void UiManager::drawCurrentPage(uint32_t nowMs) {
  tft_.fillScreen(cBg_);
  tft_.setTextWrap(false);
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
    case Page::ABOUT: drawAbout(); break;
    case Page::FAULT: drawFault(); break;
    case Page::DELETE_CONFIRM: drawDeleteConfirm(); break;
  }
}

void UiManager::drawHeader(const char *title, const char *status,
                           uint16_t accent) {
  tft_.fillRoundRect(6, 5, 228, 27, 8, cPanel_);
  tft_.setTextSize(2);
  tft_.setTextColor(cText_);
  tft_.setCursor(14, 11);
  tft_.print(title);

  if (status != nullptr) {
    tft_.setTextSize(1);
    const int16_t width = static_cast<int16_t>(strlen(status) * 6 + 12);
    const int16_t x = 228 - width;
    tft_.fillRoundRect(x, 10, width, 16, 6,
                       accent == 0 ? cCyan_ : accent);
    tft_.setTextColor(cBg_);
    tft_.setCursor(x + 6, 14);
    tft_.print(status);
  }
}

void UiManager::drawButtons(const char *left, const char *middle,
                            const char *right) {
  const char *labels[3] = {left, middle, right};
  const int16_t xs[3] = {7, 84, 161};
  for (uint8_t i = 0; i < 3; ++i) {
    tft_.fillRoundRect(xs[i], BUTTON_Y, 72, 20, 6, cPanel2_);
    tft_.drawRoundRect(xs[i], BUTTON_Y, 72, 20, 6, cLine_);
    tft_.setTextSize(1);
    tft_.setTextColor(cText_);
    const int16_t textWidth = static_cast<int16_t>(strlen(labels[i]) * 6);
    tft_.setCursor(xs[i] + (72 - textWidth) / 2, BUTTON_Y + 6);
    tft_.print(labels[i]);
  }
}

void UiManager::drawPanel(int16_t x, int16_t y, int16_t w, int16_t h,
                          bool selected, uint16_t accent) {
  tft_.fillRoundRect(x, y, w, h, 9, selected ? cPanel2_ : cPanel_);
  tft_.drawRoundRect(x, y, w, h, 9,
                     selected ? (accent == 0 ? cCyan_ : accent) : cLine_);
  if (selected) {
    tft_.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 8,
                       accent == 0 ? cCyan_ : accent);
  }
}

void UiManager::drawCentered(const char *text, int16_t y, uint8_t size,
                             uint16_t color) {
  tft_.setTextSize(size);
  tft_.setTextColor(color);
  const int16_t textWidth = static_cast<int16_t>(strlen(text) * 6 * size);
  tft_.setCursor((SCREEN_W - textWidth) / 2, y);
  tft_.print(text);
}

void UiManager::drawTemperature(float temperatureC, int16_t x, int16_t y,
                                uint8_t size, uint16_t color) {
  char value[12];
  if (std::isfinite(temperatureC)) {
    snprintf(value, sizeof(value), "%d", static_cast<int>(roundf(temperatureC)));
  } else {
    strlcpy(value, "---", sizeof(value));
  }

  tft_.setTextSize(size);
  tft_.setTextColor(color);
  tft_.setCursor(x, y);
  tft_.print(value);
  const int16_t valueWidth = static_cast<int16_t>(strlen(value) * 6 * size);
  const int16_t degreeX = x + valueWidth + 2;
  tft_.drawCircle(degreeX + 2, y + 3, (size / 2U) > 1U ? static_cast<int16_t>(size / 2U) : 1, color);
  tft_.setTextSize((size / 2U) > 1U ? static_cast<uint8_t>(size / 2U) : 1U);
  tft_.setCursor(degreeX + 7, y + 2);
  tft_.print("C");
}

void UiManager::drawProgress(int16_t x, int16_t y, int16_t w, int16_t h,
                             float fraction, uint16_t color) {
  fraction = constrain(fraction, 0.0f, 1.0f);
  tft_.fillRoundRect(x, y, w, h, h / 2, tft_.color565(18, 22, 31));
  tft_.drawRoundRect(x, y, w, h, h / 2, cLine_);
  const int16_t fillWidth = static_cast<int16_t>((w - 2) * fraction);
  if (fillWidth > 2) {
    tft_.fillRoundRect(x + 1, y + 1, fillWidth, h - 2,
                       ((h - 2) / 2) > 1 ? static_cast<int16_t>((h - 2) / 2) : 1, color);
  }
}

void UiManager::drawListRow(int16_t y, const char *primary,
                            const char *secondary, bool selected,
                            uint16_t dotColor) {
  drawPanel(12, y, 216, 44, selected, selected ? cCyan_ : 0);
  tft_.setTextColor(cText_);
  tft_.setTextSize(2);
  tft_.setCursor(24, y + 7);
  tft_.print(primary);
  if (secondary != nullptr) {
    tft_.setTextColor(cMuted_);
    tft_.setTextSize(1);
    tft_.setCursor(24, y + 29);
    tft_.print(secondary);
  }
  if (dotColor != 0) {
    tft_.fillCircle(205, y + 21, 7, dotColor);
  }
}

void UiManager::drawScrollIndicator(uint8_t totalItems, uint8_t firstVisible,
                                    uint8_t visibleItems) {
  if (totalItems <= visibleItems || totalItems == 0) return;
  tft_.fillRoundRect(232, 43, 4, 160, 2, cPanel2_);
  const int16_t thumbHeight = (160 * visibleItems / totalItems) > 16 ? static_cast<int16_t>(160 * visibleItems / totalItems) : 16;
  const int16_t travel = 160 - thumbHeight;
  const int16_t maxFirst = totalItems - visibleItems;
  const int16_t thumbY = 43 + (maxFirst == 0 ? 0 : travel * firstVisible / maxFirst);
  tft_.fillRoundRect(232, thumbY, 4, thumbHeight, 2, cCyan_);
}

void UiManager::drawProfileGraph(const ReflowProfile &profile, int16_t x,
                                 int16_t y, int16_t w, int16_t h) {
  tft_.fillRoundRect(x, y, w, h, 8, tft_.color565(13, 16, 23));
  tft_.drawRoundRect(x, y, w, h, 8, cLine_);
  for (uint8_t i = 1; i < 4; ++i) {
    const int16_t gy = y + i * h / 4;
    tft_.drawFastHLine(x + 4, gy, w - 8, tft_.color565(35, 42, 56));
  }
  for (uint8_t i = 1; i < 5; ++i) {
    const int16_t gx = x + i * w / 5;
    tft_.drawFastVLine(gx, y + 4, h - 8, tft_.color565(35, 42, 56));
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
      tft_.drawLine(previousX, holdStartY, nextX, holdStartY, cMuted_);
    } else {
      tft_.drawLine(previousX, previousY, nextX, nextY, cMuted_);
    }
    previousX = nextX;
    previousY = nextY;
    previousTemp = nextTemp;
  }
}

void UiManager::drawRunGraph(int16_t x, int16_t y, int16_t w, int16_t h) {
  tft_.fillRoundRect(x, y, w, h, 8, tft_.color565(13, 16, 23));
  tft_.drawRoundRect(x, y, w, h, 8, cLine_);
  for (uint8_t i = 1; i < 4; ++i) {
    tft_.drawFastHLine(x + 4, y + i * h / 4, w - 8,
                       tft_.color565(35, 42, 56));
  }
  for (uint8_t i = 1; i < 5; ++i) {
    tft_.drawFastVLine(x + i * w / 5, y + 4, h - 8,
                       tft_.color565(35, 42, 56));
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
      tft_.drawLine(lastTargetX, lastTargetY, px, targetY, cMuted_);
      tft_.drawLine(lastActualX, lastActualY, px, actualY, cCyan_);
      tft_.drawLine(lastActualX, lastActualY + 1, px, actualY + 1, cCyan_);
    }
    lastActualX = px;
    lastActualY = actualY;
    lastTargetX = px;
    lastTargetY = targetY;
  }
}

void UiManager::drawHome() {
  const bool ready = sensor_.valid() && !safetyEstopLatched();
  drawHeader("REFLOW OVEN", ready ? "READY" : "LOCKED",
             ready ? cGreen_ : cRed_);

  drawPanel(12, 42, 216, 82);
  const float temp = sensor_.valid() ? sensor_.temperatureC() : NAN;
  drawTemperature(temp, 52, 53, 5, sensor_.valid() ? cCyan_ : cRed_);
  drawCentered("Chamber temperature", 103, 1, cMuted_);

  drawPanel(12, 133, 216, 72);
  tft_.setTextSize(1);
  tft_.setTextColor(cMuted_);
  tft_.setCursor(24, 145);
  tft_.print("Selected profile");

  const ReflowProfile &profile = profiles_.selectedProfile();
  tft_.setTextSize(2);
  tft_.setTextColor(cText_);
  tft_.setCursor(24, 164);
  tft_.print(profile.name);

  char detail[48];
  snprintf(detail, sizeof(detail), "Peak %.0fC   TAL %us",
           profilePeakTargetC(profile),
           profile.targetTimeAboveLiquidusS);
  tft_.setTextSize(1);
  tft_.setTextColor(cYellow_);
  tft_.setCursor(24, 191);
  tft_.print(detail);

  drawButtons("PROFILE", "START", "MENU");
}

void UiManager::drawProfileList() {
  drawHeader("PROFILES");
  const uint8_t total = profiles_.profileCount() + 1U;
  cursor_ = clampCursor(cursor_, total);
  uint8_t first = 0;
  if (cursor_ >= VISIBLE_PROFILE_ROWS) {
    first = cursor_ - VISIBLE_PROFILE_ROWS + 1U;
  }

  for (uint8_t row = 0; row < VISIBLE_PROFILE_ROWS; ++row) {
    const uint8_t index = first + row;
    if (index >= total) break;
    const int16_t y = 44 + row * 52;
    if (index == profiles_.profileCount()) {
      drawListRow(y, "+ Add profile", "Duplicate then edit",
                  index == cursor_, cPurple_);
    } else {
      const ReflowProfile &profile = profiles_.profile(index);
      char secondary[40];
      snprintf(secondary, sizeof(secondary), "Liq %.0fC  max %.0fC",
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
  tft_.setTextSize(1);
  tft_.setTextColor(cMuted_);
  tft_.setCursor(18, 142);
  tft_.print("Liquidus");
  snprintf(line, sizeof(line), "%.0fC", profile.liquidusC);
  tft_.setTextColor(cText_);
  tft_.setCursor(82, 142);
  tft_.print(line);

  tft_.setTextColor(cMuted_);
  tft_.setCursor(18, 159);
  tft_.print("Max limit");
  snprintf(line, sizeof(line), "%.0fC", profile.maxTemperatureC);
  tft_.setTextColor(cText_);
  tft_.setCursor(82, 159);
  tft_.print(line);

  tft_.setTextColor(cMuted_);
  tft_.setCursor(18, 176);
  tft_.print("Ramp limit");
  snprintf(line, sizeof(line), "%.1fC/s", profile.maxRampCPerSecond);
  tft_.setTextColor(cText_);
  tft_.setCursor(82, 176);
  tft_.print(line);

  tft_.setTextColor(cMuted_);
  tft_.setCursor(18, 193);
  tft_.print("Stages");
  snprintf(line, sizeof(line), "%u", profile.stageCount);
  tft_.setTextColor(cYellow_);
  tft_.setCursor(82, 193);
  tft_.print(line);

  drawButtons("BACK", "EDIT", "START");
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
    tft_.setTextSize(1);
    tft_.setTextColor(index == cursor_ ? cText_ : cMuted_);
    tft_.setCursor(23, y + 9);
    tft_.print(labels[index]);

    char value[32] = "";
    switch (index) {
      case 0: strlcpy(value, editProfile_.name, sizeof(value)); break;
      case 1: snprintf(value, sizeof(value), "%.0fC", editProfile_.liquidusC); break;
      case 2: snprintf(value, sizeof(value), "%.0fC", editProfile_.maxTemperatureC); break;
      case 3: snprintf(value, sizeof(value), "%.1fC/s", editProfile_.maxRampCPerSecond); break;
      case 4: snprintf(value, sizeof(value), "%us", editProfile_.targetTimeAboveLiquidusS); break;
      case 5: snprintf(value, sizeof(value), "%u", editProfile_.stageCount); break;
      case 6: strlcpy(value, profiles_.profileCount() > 1 ? "Available" : "Locked", sizeof(value)); break;
      case 7: strlcpy(value, "Save", sizeof(value)); break;
    }
    const int16_t valueWidth = static_cast<int16_t>(strlen(value) * 6);
    tft_.setTextColor(index == 6 ? cRed_ : (index == 7 ? cGreen_ : cText_));
    tft_.setCursor(216 - valueWidth, y + 9);
    tft_.print(value);
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
      snprintf(secondary, sizeof(secondary), "%s  %.0fC  %us",
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
    tft_.setTextSize(1);
    tft_.setTextColor(index == cursor_ ? cText_ : cMuted_);
    tft_.setCursor(23, y + 9);
    tft_.print(labels[index]);

    char value[32] = "";
    switch (index) {
      case 0: strlcpy(value, stage.name, sizeof(value)); break;
      case 1: strlcpy(value, stageModeName(stage.mode), sizeof(value)); break;
      case 2: snprintf(value, sizeof(value), "%.0fC", stage.targetC); break;
      case 3: snprintf(value, sizeof(value), "%us", stage.durationS); break;
      case 4: strlcpy(value, editStageIndex_ > 0 ? "Available" : "Top", sizeof(value)); break;
      case 5: strlcpy(value, editStageIndex_ + 1U < editProfile_.stageCount ? "Available" : "Bottom", sizeof(value)); break;
      case 6: strlcpy(value, editProfile_.stageCount > 1 ? "Delete" : "Locked", sizeof(value)); break;
      case 7: strlcpy(value, "Done", sizeof(value)); break;
    }
    const int16_t valueWidth = static_cast<int16_t>(strlen(value) * 6);
    tft_.setTextColor(index == 6 ? cRed_ : (index == 7 ? cGreen_ : cText_));
    tft_.setCursor(216 - valueWidth, y + 9);
    tft_.print(value);
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
      snprintf(value, sizeof(value), "%.0f C", editProfile_.liquidusC); break;
    case ValueKind::MAX_TEMPERATURE:
      title = "MAX TEMPERATURE"; label = "Safety ceiling";
      snprintf(value, sizeof(value), "%.0f C", editProfile_.maxTemperatureC); break;
    case ValueKind::MAX_RAMP:
      title = "RAMP LIMIT"; label = "Maximum target ramp";
      snprintf(value, sizeof(value), "%.1f C/s", editProfile_.maxRampCPerSecond); break;
    case ValueKind::TAL_TARGET:
      title = "TAL TARGET"; label = "Above liquidus";
      snprintf(value, sizeof(value), "%u seconds", editProfile_.targetTimeAboveLiquidusS); break;
    case ValueKind::STAGE_TARGET:
      title = "STAGE TARGET"; label = editProfile_.stages[editStageIndex_].name;
      snprintf(value, sizeof(value), "%.0f C", editProfile_.stages[editStageIndex_].targetC); break;
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
  drawCentered(buffer, 75, 2, cText_);

  const int16_t charX = 120 - static_cast<int16_t>(strlen(buffer) * 6 * 2) / 2 +
                        nameCursor_ * 12;
  tft_.drawFastHLine(charX, 98, 10, cPurple_);
  tft_.setTextSize(1);
  tft_.setTextColor(cMuted_);
  tft_.setCursor(18, 148);
  tft_.print("Left/right change character");
  tft_.setCursor(18, 165);
  tft_.print("OK advances cursor");
  tft_.setTextColor(cYellow_);
  tft_.setCursor(18, 188);
  tft_.print("Hold OK to save name");
  drawButtons("CHAR-", "NEXT", "CHAR+");
}

void UiManager::drawRunning(uint32_t nowMs) {
  const bool paused = engine_.state() == RunState::PAUSED;
  drawHeader("RUNNING", paused ? "PAUSED" : engine_.stageName(),
             paused ? cPurple_ : cYellow_);

  drawPanel(12, 40, 100, 62);
  tft_.setTextSize(1);
  tft_.setTextColor(cMuted_);
  tft_.setCursor(22, 48);
  tft_.print("ACTUAL");
  drawTemperature(sensor_.valid() ? sensor_.temperatureC() : NAN,
                  22, 64, 3, cCyan_);

  drawPanel(128, 40, 100, 62);
  tft_.setTextSize(1);
  tft_.setTextColor(cMuted_);
  tft_.setCursor(138, 48);
  tft_.print("TARGET");
  drawTemperature(engine_.targetTemperatureC(), 138, 64, 3, cYellow_);

  drawRunGraph(12, 111, 216, 70);

  char elapsed[12];
  char total[12];
  formatTime(engine_.runElapsedMs(nowMs) / 1000UL, elapsed, sizeof(elapsed));
  formatTime(engine_.expectedDurationMs() / 1000UL, total, sizeof(total));
  char timeLine[28];
  snprintf(timeLine, sizeof(timeLine), "%s / %s", elapsed, total);
  tft_.setTextSize(1);
  tft_.setTextColor(cText_);
  tft_.setCursor(18, 190);
  tft_.print(timeLine);
  drawProgress(112, 188, 110, 13, engine_.progress(nowMs), cGreen_);

  drawButtons("STOP", paused ? "RESUME" : "PAUSE", "INFO");
}

void UiManager::drawRunInfo(uint32_t nowMs) {
  drawHeader("RUN DETAILS", engine_.stageName(), cYellow_);
  const ReflowProfile &profile = engine_.activeProfile();
  const ReflowStage &stage = profile.stages[engine_.stageIndex() < (profile.stageCount - 1U)
                            ? engine_.stageIndex()
                            : static_cast<uint8_t>(profile.stageCount - 1U)];

  drawPanel(12, 43, 216, 151);
  char line[48];
  tft_.setTextSize(1);
  tft_.setTextColor(cMuted_);
  tft_.setCursor(22, 54);
  tft_.print("Profile");
  tft_.setTextColor(cText_);
  tft_.setCursor(92, 54);
  tft_.print(profile.name);

  tft_.setTextColor(cMuted_);
  tft_.setCursor(22, 76);
  tft_.print("Stage mode");
  tft_.setTextColor(cText_);
  tft_.setCursor(92, 76);
  tft_.print(stageModeName(stage.mode));

  tft_.setTextColor(cMuted_);
  tft_.setCursor(22, 98);
  tft_.print("Heater");
  snprintf(line, sizeof(line), "%.0f%%", engine_.heaterDemandPercent());
  tft_.setTextColor(cOrange_);
  tft_.setCursor(92, 98);
  tft_.print(line);

  tft_.setTextColor(cMuted_);
  tft_.setCursor(22, 120);
  tft_.print("Peak");
  snprintf(line, sizeof(line), "%.1f C", engine_.peakTemperatureC());
  tft_.setTextColor(cText_);
  tft_.setCursor(92, 120);
  tft_.print(line);

  tft_.setTextColor(cMuted_);
  tft_.setCursor(22, 142);
  tft_.print("TAL actual");
  snprintf(line, sizeof(line), "%lus",
           static_cast<unsigned long>(engine_.timeAboveLiquidusMs() / 1000UL));
  tft_.setTextColor(cYellow_);
  tft_.setCursor(92, 142);
  tft_.print(line);

  tft_.setTextColor(cMuted_);
  tft_.setCursor(22, 164);
  tft_.print("Elapsed");
  formatTime(engine_.runElapsedMs(nowMs) / 1000UL, line, sizeof(line));
  tft_.setTextColor(cText_);
  tft_.setCursor(92, 164);
  tft_.print(line);

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
  tft_.setTextSize(1);
  tft_.setTextColor(cText_);
  tft_.setCursor(23, 153);
  tft_.print(line);
  snprintf(line, sizeof(line), "Time above liquidus: %lus",
           static_cast<unsigned long>(engine_.timeAboveLiquidusMs() / 1000UL));
  tft_.setCursor(23, 172);
  tft_.print(line);
  tft_.setTextColor(cYellow_);
  tft_.setCursor(23, 192);
  tft_.print("Do not handle PCB while hot");
  drawButtons("HOME", "LOG", "REPEAT");
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
    tft_.setTextSize(1);
    tft_.setTextColor(cText_);
    tft_.setCursor(28, y + 9);
    tft_.print(items[i]);
  }
  drawButtons("BACK", "OPEN", "DOWN");
}

void UiManager::drawManual() {
  const bool active = engine_.state() == RunState::MANUAL;
  drawHeader("MANUAL HEAT", active ? "ON" : "OFF",
             active ? cOrange_ : cRed_);
  drawPanel(12, 45, 216, 83);
  drawTemperature(manualSetpointC_, 48, 57, 5, cOrange_);
  drawCentered("Setpoint", 106, 1, cMuted_);

  char line[48];
  snprintf(line, sizeof(line), "Actual: %.1f C",
           sensor_.valid() ? sensor_.temperatureC() : NAN);
  tft_.setTextSize(1);
  tft_.setTextColor(cText_);
  tft_.setCursor(20, 145);
  tft_.print(line);
  drawProgress(20, 166, 200, 15,
               active ? engine_.heaterDemandPercent() / 100.0f : 0.0f,
               cRed_);
  snprintf(line, sizeof(line), "Heater output: %.0f%%",
           active ? engine_.heaterDemandPercent() : 0.0f);
  tft_.setTextColor(cMuted_);
  tft_.setCursor(20, 190);
  tft_.print(line);
  drawButtons("-", active ? "OFF" : "ON", "+");
}

void UiManager::drawCalibration() {
  drawHeader("CALIBRATION");
  drawPanel(12, 48, 216, 102, true, cCyan_);
  char value[24];
  snprintf(value, sizeof(value), "%+.1f C", calibrationWorkingC_);
  drawCentered(value, 72, 4, cCyan_);
  drawCentered("PT100 correction offset", 125, 1, cMuted_);

  tft_.setTextSize(1);
  tft_.setTextColor(cYellow_);
  tft_.setCursor(18, 169);
  tft_.print("Calibrate against a trusted probe");
  tft_.setTextColor(cMuted_);
  tft_.setCursor(18, 188);
  tft_.print("Adjustment step: 0.1 C");
  drawButtons("-", "SAVE", "+");
}

void UiManager::drawLogs() {
  drawHeader("RUN LOGS");
  const uint8_t count = profiles_.runLogCount();
  if (count == 0) {
    drawPanel(12, 50, 216, 116);
    drawCentered("No completed runs", 88, 2, cMuted_);
    drawCentered("Logs appear after reflow", 122, 1, cMuted_);
  } else {
    logCursor_ = clampCursor(logCursor_, count);
    const RunSummary &log = profiles_.runLogNewest(logCursor_);
    drawPanel(12, 43, 216, 154);
    tft_.setTextSize(2);
    tft_.setTextColor(cText_);
    tft_.setCursor(22, 54);
    tft_.print(log.profileName);

    char line[48];
    snprintf(line, sizeof(line), "Peak %.1f C", log.peakTemperatureC);
    tft_.setTextSize(1);
    tft_.setTextColor(cCyan_);
    tft_.setCursor(22, 89);
    tft_.print(line);
    snprintf(line, sizeof(line), "TAL %us", log.timeAboveLiquidusS);
    tft_.setCursor(22, 111);
    tft_.print(line);
    snprintf(line, sizeof(line), "Total %us", log.totalTimeS);
    tft_.setCursor(22, 133);
    tft_.print(line);
    snprintf(line, sizeof(line), "Log %u of %u", logCursor_ + 1U, count);
    tft_.setTextColor(cMuted_);
    tft_.setCursor(22, 171);
    tft_.print(line);
  }
  drawButtons("BACK", "HOME", "NEXT");
}

void UiManager::drawSettings() {
  drawHeader("SETTINGS");
  static const char *items[] = {
      "Button buzzer", "Fan during cool", "Backlight", "Reset profiles",
      "About", "Back"};
  constexpr uint8_t count = sizeof(items) / sizeof(items[0]);
  cursor_ = clampCursor(cursor_, count);
  uint8_t first = 0;
  if (cursor_ >= VISIBLE_EDIT_ROWS) first = cursor_ - VISIBLE_EDIT_ROWS + 1U;

  for (uint8_t row = 0; row < VISIBLE_EDIT_ROWS; ++row) {
    const uint8_t i = first + row;
    if (i >= count) break;
    const int16_t y = 44 + row * 32;
    drawPanel(16, y, 208, 27, i == cursor_);
    tft_.setTextSize(1);
    tft_.setTextColor(cText_);
    tft_.setCursor(28, y + 9);
    tft_.print(items[i]);

    char value[16] = "";
    if (i == 0) strlcpy(value, profiles_.settings().buzzerEnabled ? "ON" : "OFF", sizeof(value));
    if (i == 1) strlcpy(value, profiles_.settings().fanDuringCool ? "ON" : "OFF", sizeof(value));
    if (i == 2) snprintf(value, sizeof(value), "%u%%", profiles_.settings().backlightPercent);
    if (i == 3) strlcpy(value, "RESTORE", sizeof(value));
    if (i == 4) strlcpy(value, "OPEN", sizeof(value));
    if (i == 5) strlcpy(value, "DONE", sizeof(value));
    tft_.setTextColor(i == 3 ? cRed_ : (i == 5 ? cGreen_ : cMuted_));
    tft_.setCursor(208 - static_cast<int16_t>(strlen(value) * 6), y + 9);
    tft_.print(value);
  }
  drawScrollIndicator(count, first, VISIBLE_EDIT_ROWS);
  drawButtons("BACK", "CHANGE", "DOWN");
}

void UiManager::drawAbout() {
  drawHeader("ABOUT");
  drawPanel(12, 44, 216, 157);
  drawCentered("Universal Reflow", 57, 2, cCyan_);
  drawCentered("Controller v1.3", 79, 2, cText_);

  tft_.setTextSize(1);
  tft_.setTextColor(cMuted_);
  tft_.setCursor(22, 113);
  tft_.print("ESP32-S3-WROOM-1-N16");
  tft_.setCursor(22, 132);
  tft_.print("ST7789 240x240 + MAX31865");
  tft_.setCursor(22, 151);
  tft_.print("Profiles stored in NVS flash");
  tft_.setTextColor(cYellow_);
  tft_.setCursor(22, 178);
  tft_.print("Use a thermal fuse and enclosure");
  drawButtons("BACK", "HOME", "BACK");
}

void UiManager::drawFault() {
  drawHeader("FAULT", "STOP", cRed_);
  tft_.fillRoundRect(12, 45, 216, 129, 14, tft_.color565(45, 20, 26));
  tft_.drawRoundRect(12, 45, 216, 129, 14, cRed_);
  drawCentered("!", 55, 6, cRed_);
  drawCentered(faultCodeName(engine_.faultCode()), 117, 1, cText_);
  drawCentered(engine_.faultDetail(), 139, 1, cMuted_);

  tft_.setTextSize(1);
  tft_.setTextColor(cYellow_);
  tft_.setCursor(22, 187);
  if (engine_.faultCode() == FaultCode::ESTOP && !safetyEstopCircuitHealthy()) {
    tft_.print("Release E-stop / repair circuit");
  } else {
    tft_.print("Hold RESET to clear fault");
  }
  drawButtons("LOCKED", "DETAIL", "HOLD RST");
}

void UiManager::drawDeleteConfirm() {
  drawHeader("DELETE PROFILE", "CONFIRM", cRed_);
  drawPanel(12, 51, 216, 116, true, cRed_);
  drawCentered("Delete this profile?", 70, 2, cText_);
  drawCentered(editProfile_.name, 105, 2, cRed_);
  drawCentered("This cannot be undone", 145, 1, cYellow_);
  drawButtons("CANCEL", "DELETE", "CANCEL");
}

void UiManager::handleHome(const ButtonEvent &event, uint32_t nowMs) {
  if (isPress(event, ButtonId::LEFT)) {
    cursor_ = profiles_.selectedIndex();
    page_ = Page::PROFILE_LIST;
  } else if (isPress(event, ButtonId::MIDDLE)) {
    startSelectedProfile(nowMs);
  } else if (isPress(event, ButtonId::RIGHT)) {
    cursor_ = 0;
    page_ = Page::MENU;
  }
}

void UiManager::handleProfileList(const ButtonEvent &event) {
  const uint8_t total = profiles_.profileCount() + 1U;
  if (isPress(event, ButtonId::LEFT)) {
    page_ = Page::HOME;
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    cursor_ = static_cast<uint8_t>((cursor_ + 1U) % total);
  } else if (isPress(event, ButtonId::MIDDLE)) {
    if (cursor_ == profiles_.profileCount()) {
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
  } else if (isPress(event, ButtonId::RIGHT)) {
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
    if (sensor_.valid() && !safetyEstopLatched()) {
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
    } else if (sensor_.valid() && !safetyEstopLatched()) {
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
  constexpr uint8_t itemCount = 6;
  if (isPress(event, ButtonId::LEFT)) {
    cursor_ = 3;
    page_ = Page::MENU;
  } else if (isAdjust(event, ButtonId::RIGHT)) {
    cursor_ = static_cast<uint8_t>((cursor_ + 1U) % itemCount);
  } else if (isPress(event, ButtonId::MIDDLE)) {
    switch (cursor_) {
      case 0:
        profiles_.settings().buzzerEnabled = !profiles_.settings().buzzerEnabled;
        profiles_.save();
        break;
      case 1:
        profiles_.settings().fanDuringCool = !profiles_.settings().fanDuringCool;
        profiles_.save();
        break;
      case 2: {
        uint8_t brightness = profiles_.settings().backlightPercent;
        brightness = static_cast<uint8_t>(brightness + TFT_BACKLIGHT_STEP_PERCENT);
        if (brightness > 100U) brightness = TFT_BACKLIGHT_MIN_PERCENT;
        profiles_.settings().backlightPercent = brightness;
        backlight_.setPercent(brightness);
        profiles_.save();
        break;
      }
      case 3:
        profiles_.resetDefaults();
        profiles_.save();
        sensor_.setCalibrationOffset(profiles_.settings().temperatureOffsetC);
        backlight_.setPercent(profiles_.settings().backlightPercent);
        break;
      case 4:
        page_ = Page::ABOUT;
        break;
      case 5:
        cursor_ = 3;
        page_ = Page::MENU;
        break;
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
  if (event.button != ButtonId::RIGHT ||
      event.action != ButtonAction::LONG_PRESS) {
    return;
  }

  if (engine_.faultCode() == FaultCode::ESTOP) {
    if (!safetyResetEstopLatch()) return;
  }
  if (engine_.clearFault()) {
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
  if (safetyEstopLatched()) {
    engine_.triggerFault(FaultCode::ESTOP, "E-stop circuit is open");
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

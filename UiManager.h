#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include "BacklightController.h"
#include "ButtonInput.h"
#include "ProfileStore.h"
#include "ReflowEngine.h"
#include "TemperatureSensor.h"

class UiManager {
 public:
  UiManager(Adafruit_ST7789 &display, ProfileStore &profiles,
            ReflowEngine &engine, TemperatureSensor &sensor,
            BacklightController &backlight);

  void begin();
  void update(uint32_t nowMs);
  void handleButton(const ButtonEvent &event, uint32_t nowMs);
  void markDirty() { dirty_ = true; }

 private:
  enum class Page : uint8_t {
    HOME,
    PROFILE_LIST,
    PROFILE_DETAIL,
    PROFILE_EDIT,
    STAGE_LIST,
    STAGE_EDIT,
    VALUE_EDIT,
    NAME_EDIT,
    RUNNING,
    RUN_INFO,
    COMPLETE,
    MENU,
    MANUAL,
    CALIBRATION,
    LOGS,
    SETTINGS,
    ABOUT,
    FAULT,
    DELETE_CONFIRM,
  };

  enum class ValueKind : uint8_t {
    LIQUIDUS,
    MAX_TEMPERATURE,
    MAX_RAMP,
    TAL_TARGET,
    STAGE_TARGET,
    STAGE_DURATION,
  };

  enum class NameKind : uint8_t {
    PROFILE,
    STAGE,
  };

  Adafruit_ST7789 &tft_;
  ProfileStore &profiles_;
  ReflowEngine &engine_;
  TemperatureSensor &sensor_;
  BacklightController &backlight_;

  Page page_ = Page::HOME;
  Page valueReturnPage_ = Page::PROFILE_EDIT;
  Page nameReturnPage_ = Page::PROFILE_EDIT;
  RunState lastRunState_ = RunState::IDLE;
  uint8_t cursor_ = 0;
  uint8_t editProfileIndex_ = 0;
  uint8_t editStageIndex_ = 0;
  uint8_t nameCursor_ = 0;
  uint8_t logCursor_ = 0;
  ValueKind valueKind_ = ValueKind::LIQUIDUS;
  NameKind nameKind_ = NameKind::PROFILE;
  ReflowProfile editProfile_{};
  float calibrationWorkingC_ = 0.0f;
  float manualSetpointC_ = 120.0f;

  bool dirty_ = true;
  uint32_t lastDrawMs_ = 0;
  uint32_t buzzerOffMs_ = 0;

  uint16_t cBg_;
  uint16_t cPanel_;
  uint16_t cPanel2_;
  uint16_t cLine_;
  uint16_t cText_;
  uint16_t cMuted_;
  uint16_t cCyan_;
  uint16_t cGreen_;
  uint16_t cYellow_;
  uint16_t cOrange_;
  uint16_t cRed_;
  uint16_t cPurple_;
  uint16_t cBlue_;

  void syncPageToRunState();
  void drawCurrentPage(uint32_t nowMs);

  void drawHome();
  void drawProfileList();
  void drawProfileDetail();
  void drawProfileEdit();
  void drawStageList();
  void drawStageEdit();
  void drawValueEdit();
  void drawNameEdit();
  void drawRunning(uint32_t nowMs);
  void drawRunInfo(uint32_t nowMs);
  void drawComplete();
  void drawMenu();
  void drawManual();
  void drawCalibration();
  void drawLogs();
  void drawSettings();
  void drawAbout();
  void drawFault();
  void drawDeleteConfirm();

  void drawHeader(const char *title, const char *status = nullptr,
                  uint16_t accent = 0);
  void drawButtons(const char *left, const char *middle, const char *right);
  void drawPanel(int16_t x, int16_t y, int16_t w, int16_t h,
                 bool selected = false, uint16_t accent = 0);
  void drawCentered(const char *text, int16_t y, uint8_t size,
                    uint16_t color);
  void drawTemperature(float temperatureC, int16_t x, int16_t y,
                       uint8_t size, uint16_t color);
  void drawProgress(int16_t x, int16_t y, int16_t w, int16_t h,
                    float fraction, uint16_t color);
  void drawProfileGraph(const ReflowProfile &profile, int16_t x, int16_t y,
                        int16_t w, int16_t h);
  void drawRunGraph(int16_t x, int16_t y, int16_t w, int16_t h);
  void drawListRow(int16_t y, const char *primary, const char *secondary,
                   bool selected, uint16_t dotColor = 0);
  void drawScrollIndicator(uint8_t totalItems, uint8_t firstVisible,
                           uint8_t visibleItems);

  void handleHome(const ButtonEvent &event, uint32_t nowMs);
  void handleProfileList(const ButtonEvent &event);
  void handleProfileDetail(const ButtonEvent &event, uint32_t nowMs);
  void handleProfileEdit(const ButtonEvent &event);
  void handleStageList(const ButtonEvent &event);
  void handleStageEdit(const ButtonEvent &event);
  void handleValueEdit(const ButtonEvent &event);
  void handleNameEdit(const ButtonEvent &event);
  void handleRunning(const ButtonEvent &event, uint32_t nowMs);
  void handleRunInfo(const ButtonEvent &event, uint32_t nowMs);
  void handleComplete(const ButtonEvent &event, uint32_t nowMs);
  void handleMenu(const ButtonEvent &event);
  void handleManual(const ButtonEvent &event, uint32_t nowMs);
  void handleCalibration(const ButtonEvent &event);
  void handleLogs(const ButtonEvent &event);
  void handleSettings(const ButtonEvent &event);
  void handleAbout(const ButtonEvent &event);
  void handleFault(const ButtonEvent &event);
  void handleDeleteConfirm(const ButtonEvent &event);

  void beginProfileEdit();
  void saveProfileEdit();
  void beginValueEdit(ValueKind kind, Page returnPage);
  void adjustValue(int direction);
  void beginNameEdit(NameKind kind, Page returnPage);
  char *activeNameBuffer();
  size_t activeNameCapacity() const;
  void cycleNameCharacter(int direction);
  void addStage();
  void deleteStage();
  void moveStage(int direction);
  bool startSelectedProfile(uint32_t nowMs);
  void beep(uint16_t durationMs = 35);
  static bool isPress(const ButtonEvent &event, ButtonId id);
  static bool isAdjust(const ButtonEvent &event, ButtonId id);
};
